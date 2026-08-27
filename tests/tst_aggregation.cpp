/// @file tests/tst_aggregation.cpp
/// @brief Cross-bank upsert, dedupe and normalization tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "domain/bank.h"
#include "services/account_service.h"
#include "services/normalization.h"
#include <QtTest>

using namespace kmx;

class tst_Aggregation final: public QObject
{
    Q_OBJECT

private slots:
    void merchant_cleanup();
    void category_inference();
    void account_upsert_updates_in_place();
    void transaction_merge_dedupes_and_orders();
};

void tst_Aggregation::merchant_cleanup()
{
    QCOMPARE(clean_merchant_name(QStringLiteral("KAUFLAND RO 0123")), QStringLiteral("Kaufland"));
    QCOMPARE(clean_merchant_name(QStringLiteral("  bolt   ride ")), QStringLiteral("Bolt Ride"));
    QCOMPARE(clean_merchant_name(QString()), QString());
}

void tst_Aggregation::category_inference()
{
    QCOMPARE(infer_category_from_merchant(QStringLiteral("KAUFLAND ROMANIA 0123")), txn_category::groceries);
    QCOMPARE(infer_category_from_merchant(QStringLiteral("ENEL ENERGIE RO")), txn_category::utilities);
    QCOMPARE(infer_category_from_merchant(QStringLiteral("PETROM FUEL 88")), txn_category::transport);
    QCOMPARE(infer_category_from_merchant(QStringLiteral("NETFLIX.COM")), txn_category::entertainment);
    QCOMPARE(infer_category_from_merchant(QStringLiteral("JOE'S DINER")), txn_category::other);

    QVERIFY(merchant_looks_known(QStringLiteral("LIDL SUCURSAL 4")));
    QVERIFY(!merchant_looks_known(QStringLiteral("UNCLE BOBS")));
}

void tst_Aggregation::account_upsert_updates_in_place()
{
    account_service store;

    connector::remote_account a;
    a.external_id = QStringLiteral("ACC-1");
    a.name = QStringLiteral("EUR Pocket");
    a.currency = currency_code::eur;
    a.iban = QStringLiteral("RO42TEST0000000000000000");
    a.balance_minor = 1'000;

    connector::remote_account b;
    b.external_id = QStringLiteral("ACC-2");
    b.name = QStringLiteral("RON Pocket");
    b.balance_minor = 2'000;

    store.upsert_accounts(bank_id::tbi_bank, {a, b});
    QCOMPARE(store.accounts().size(), 2);
    QCOMPARE(store.account_by_key(QStringLiteral("2:ACC-1"))->balance_minor, qint64(1'000));

    // Same identities, fresh balance: updates in place, no duplicates.
    a.balance_minor = 1'500;
    store.upsert_accounts(bank_id::tbi_bank, {a});
    QCOMPARE(store.accounts().size(), 2);
    QCOMPARE(store.account_by_key(QStringLiteral("2:ACC-1"))->balance_minor, qint64(1'500));
}

void tst_Aggregation::transaction_merge_dedupes_and_orders()
{
    account_service store;

    connector::remote_account acc;
    acc.external_id = QStringLiteral("ACC-9");
    acc.name = QStringLiteral("Main");
    store.upsert_accounts(bank_id::banca_transilvania, {acc});

    const QDateTime t1 = QDateTime(QDate(2026, 8, 20), QTime(10, 0));
    const QDateTime t2 = QDateTime(QDate(2026, 8, 24), QTime(18, 30));

    connector::remote_transaction x;
    x.external_id = QStringLiteral("TXN-1");
    x.account_external_id = QStringLiteral("ACC-9");
    x.direction = txn_direction::debit;
    x.counterparty_raw = QStringLiteral("PROFI ROMANIA 077");
    x.amount_minor = 5'400;
    x.amount_currency = currency_code::ron;
    x.posted_at = t1;
    x.category_source = category_origin::inferred;

    connector::remote_transaction y = x;
    y.external_id = QStringLiteral("TXN-2");
    y.posted_at = t2;

    QCOMPARE(store.merge_transactions(bank_id::banca_transilvania, {x, y}), 2);
    // Re-delivery of the same batch (classic sync replay) adds nothing.
    QCOMPARE(store.merge_transactions(bank_id::banca_transilvania, {x, y}), 0);
    QCOMPARE(store.transaction_count(), 2);

    // Newest first.
    QCOMPARE(store.transactions().first().posted_at, t2);

    // normalization applied: cleaned name + inferred category + provenance.
    const transaction& first = store.transactions().first();
    QCOMPARE(first.counterparty, QStringLiteral("Profi"));
    QCOMPARE(first.category, txn_category::groceries);
    QCOMPARE(first.category_source, category_origin::inferred);
    QCOMPARE(first.account_id, qint64(1)); // resolved to the stored account
}

QTEST_MAIN(tst_Aggregation)
#include "tst_aggregation.moc"
