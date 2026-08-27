/// @file tests/tst_ledger_filters.cpp
/// @brief Ledger filter composition, diacritic search and CSV tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "services/account_service.h"
#include "viewmodels/recent_transactions_model.h"
#include "viewmodels/transaction_filter_proxy.h"
#include <QFile>
#include <QtTest>

using namespace kmx;

class tst_LedgerFilters final: public QObject
{
    Q_OBJECT

private slots:
    void filters_compose_and_clear();
    void search_folds_diacritics();
    void recategorize_marks_manual_and_reloads();
    void csv_export_has_bom_and_rows();

private:
    // 3 txns: two for account A (groceries debit, salary credit),
    // one for account B (dining debit, diacritics in the name).
    struct fixture
    {
        account_service store;
        recent_transactions_model* source;
        transaction_filter_proxy proxy;

        explicit fixture(QObject* parent): source(new recent_transactions_model(store, parent)), proxy(parent)
        {
            connector::remote_account acc_a;
            acc_a.external_id = QStringLiteral("A");
            acc_a.name = QStringLiteral("KMX checking");
            connector::remote_account acc_b;
            acc_b.external_id = QStringLiteral("B");
            acc_b.name = QStringLiteral("BT main");

            store.upsert_accounts(bank_id::kmx_bank, {acc_a});
            store.upsert_accounts(bank_id::banca_transilvania, {acc_b});

            const QDateTime early(QDate(2026, 1, 10), QTime(9, 0));
            const QDateTime mid(QDate(2026, 3, 15), QTime(12, 0));
            const QDateTime late(QDate(2026, 8, 20), QTime(18, 0));

            connector::remote_transaction groceries;
            groceries.external_id = QStringLiteral("T1");
            groceries.account_external_id = QStringLiteral("A");
            groceries.direction = txn_direction::debit;
            groceries.counterparty_raw = QStringLiteral("Kaufland");
            groceries.category_source = category_origin::native;
            groceries.category = txn_category::groceries;
            groceries.amount_minor = 5'000;
            groceries.posted_at = late;

            connector::remote_transaction salary = groceries;
            salary.external_id = QStringLiteral("T2");
            salary.direction = txn_direction::credit;
            salary.counterparty_raw = QStringLiteral("NordTech salary");
            salary.category_source = category_origin::native;
            salary.category = txn_category::salary;
            salary.amount_minor = 850'000;
            salary.posted_at = mid;

            connector::remote_transaction dining = groceries;
            dining.external_id = QStringLiteral("T3");
            dining.account_external_id = QStringLiteral("B");
            dining.counterparty_raw = QStringLiteral("Bucătăria Bunicii");
            dining.category_source = category_origin::inferred;
            dining.category = txn_category::dining;
            dining.amount_minor = 7'400;
            dining.posted_at = early;

            QCOMPARE(store.merge_transactions(bank_id::kmx_bank, {groceries}), 1);
            QCOMPARE(store.merge_transactions(bank_id::banca_transilvania, {salary, dining}), 2);

            source->set_limit(0);
            proxy.setSourceModel(source);
        }
    };
};

void tst_LedgerFilters::filters_compose_and_clear()
{
    fixture f(this);

    QCOMPARE(f.proxy.rowCount(), 3);

    // Direction alone. Enum: Credit=0, Debit=1.
    f.proxy.set_direction(0); // credit
    QCOMPARE(f.proxy.rowCount(), 1);

    f.proxy.set_direction(1); // debit
    QCOMPARE(f.proxy.rowCount(), 2);

    f.proxy.set_direction(-1);

    // Category + bank composition.
    f.proxy.set_categories({static_cast<int>(txn_category::groceries)});
    f.proxy.set_bank_ids({static_cast<int>(bank_id::banca_transilvania)});
    QCOMPARE(f.proxy.rowCount(), 0); // groceries live at KMX
    f.proxy.set_bank_ids({static_cast<int>(bank_id::kmx_bank)});
    QCOMPARE(f.proxy.rowCount(), 1);

    // Date range on top.
    f.proxy.set_from_date(QDate(2026, 1, 1));
    f.proxy.set_to_date(QDate(2026, 2, 1));
    QCOMPARE(f.proxy.rowCount(), 0); // that txn is August
    f.proxy.set_to_date(QDate(2026, 12, 31));
    QCOMPARE(f.proxy.rowCount(), 1);

    // Clear resets everything.
    f.proxy.clear();
    QCOMPARE(f.proxy.rowCount(), 3);
}

void tst_LedgerFilters::search_folds_diacritics()
{
    fixture f(this);

    f.proxy.set_search_text(QStringLiteral("buca")); // no diacritics typed
    QCOMPARE(f.proxy.rowCount(), 1);

    f.proxy.set_search_text(QStringLiteral("bunici"));
    QCOMPARE(f.proxy.rowCount(), 1);

    f.proxy.set_search_text(QStringLiteral("kaufland"));
    QCOMPARE(f.proxy.rowCount(), 1);

    f.proxy.set_search_text(QStringLiteral("zzz"));
    QCOMPARE(f.proxy.rowCount(), 0);
}

void tst_LedgerFilters::recategorize_marks_manual_and_reloads()
{
    fixture f(this);

    // Find T3's composite reference ("1:BK..."): look it up from the source rows.
    QString ref_t3;
    for (const auto& t: f.source->transactions())
        if (t.counterparty.startsWith(QStringLiteral("Bucătăria")))
            ref_t3 = t.reference;
    QVERIFY(!ref_t3.isEmpty());

    QVERIFY(f.store.amend_transaction(ref_t3, static_cast<int>(txn_category::travel), QStringLiteral("trip expense")));
    QVERIFY(!f.store.amend_transaction(QStringLiteral("missing"), 0, QString()));

    // Source reloaded with Manual provenance and new category.
    bool found = false;
    for (int r = 0; r < f.source->rowCount(); ++r)
    {
        const QModelIndex idx = f.source->index(r, 0);
        if (f.source->data(idx, recent_transactions_model::reference_role).toString() != ref_t3)
            continue;
        found = true;
        QCOMPARE(f.source->data(idx, recent_transactions_model::category_role).toInt(), static_cast<int>(txn_category::travel));
        QCOMPARE(f.source->data(idx, recent_transactions_model::category_source_role).toInt(), static_cast<int>(category_origin::manual));
        QCOMPARE(f.source->data(idx, recent_transactions_model::note_role).toString(), QStringLiteral("trip expense"));
    }
    QVERIFY(found);

    // And it flows through the proxy (Analytics will read the same store).
    f.proxy.set_categories({static_cast<int>(txn_category::travel)});
    QCOMPARE(f.proxy.rowCount(), 1);
}

void tst_LedgerFilters::csv_export_has_bom_and_rows()
{
    fixture f(this);

    const QVector<transaction> rows = f.proxy.visible_transactions();
    QCOMPARE(rows.size(), 3);

    const QString path = f.store.export_csv(rows);
    QVERIFY(!path.isEmpty());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray all = file.readAll();

    QVERIFY(all.startsWith(QByteArray::fromHex("efbbbf"))); // UTF-8 BOM
    QVERIFY(all.contains("date;direction;counterparty"));
    QCOMPARE(all.count('\n') - 1, 3); // header + 3 rows

    // Semicolon delimiter survives LibreOffice's ro_RO expectations,
    // quoted fields protect the comma-free values we emit.
    QVERIFY(all.contains("\"credit\""));
    QVERIFY(path.endsWith(QStringLiteral(".csv")));
}

QTEST_MAIN(tst_LedgerFilters)
#include "tst_ledger_filters.moc"
