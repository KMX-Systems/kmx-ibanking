/// @file tests/tst_payments.cpp
/// @brief Transfer validation catalog and execution tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "domain/iban.h"
#include "services/account_service.h"
#include "services/clock_source.h"
#include "services/payment_service.h"
#include <QSignalSpy>
#include <QtTest>

using namespace kmx;
using Code = payment_error_code;

class tst_Payments final: public QObject
{
    Q_OBJECT

private slots:
    void validation_rejects_bad_amounts();
    void validation_rejects_bad_ibans();
    void insufficient_funds_and_credit_accounts();
    void self_transfer_rejected();
    void internal_cross_currency_blocked_until_advisor();
    void internal_execution_moves_balances_and_posts_both_legs();
    void external_execution_posts_single_debit();
    void scheduled_requires_capability_and_recurs();

private:
    struct world
    {
        account_service store;
        fake_clock clock {QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0))};
        payment_service payments;

        explicit world(QObject* parent): payments(store, clock)
        {
            connector::remote_account kmx_checking;
            kmx_checking.external_id = "KC";
            kmx_checking.iban = make_romanian_iban(QStringLiteral("KMXB"), 1001);
            kmx_checking.name = "KMX checking";
            kmx_checking.balance_minor = 100'000'00; // 100 000 RON

            connector::remote_account kmx_credit;
            kmx_credit.external_id = "KX";
            kmx_credit.iban = make_romanian_iban(QStringLiteral("KMXB"), 1002);
            kmx_credit.kind = account_kind::credit;
            kmx_credit.name = "KMX credit";
            kmx_credit.balance_minor = -2'000'00; // in debt

            connector::remote_account tbi_ron;
            tbi_ron.external_id = "TR";
            tbi_ron.iban = make_romanian_iban(QStringLiteral("TBI1"), 1003);
            tbi_ron.name = "TBI RON pocket";
            tbi_ron.balance_minor = 500'00;
            connector::remote_account tbi_eur = tbi_ron;
            tbi_eur.external_id = "TE";
            tbi_eur.iban = make_romanian_iban(QStringLiteral("TBI1"), 1004);
            tbi_eur.currency = currency_code::eur;
            tbi_eur.balance_minor = 300'00;

            store.upsert_accounts(bank_id::kmx_bank, {kmx_checking, kmx_credit});
            store.upsert_accounts(bank_id::tbi_bank, {tbi_ron, tbi_eur});

            // Capability matrix: KMX + BT support schedules; TBI does not.
            payments.set_capability_resolver(
                [](int bank)
                { return bank == static_cast<int>(bank_id::kmx_bank) || bank == static_cast<int>(bank_id::banca_transilvania); });
            payments.set_latency(std::chrono::milliseconds(0));
        }
    };

    static transfer_request req(qint64 src, const QString& iban, qint64 amount_minor)
    {
        transfer_request r;
        r.source_account_id = src;
        r.beneficiary_iban = iban;
        r.beneficiary_name = QStringLiteral("Test beneficiary");
        r.amount_minor = amount_minor;
        return r;
    }

    // Store ids are sequential in upsert order: 1 KC, 2 KX, 3 TR, 4 TE.
    static constexpr qint64 kmx_checking_id = 1;
    static constexpr qint64 kmx_credit_id = 2;
    static constexpr qint64 tbi_ron_id = 3;
    static constexpr qint64 tbi_eur_id = 4;

    static qint64 balance_of(const world& w, qint64 id)
    {
        for (const auto& a: w.store.accounts())
            if (a.id == id)
                return a.balance_minor;
        return -1;
    }

    static QString iban_of(const world& w, qint64 id)
    {
        for (const auto& a: w.store.accounts())
            if (a.id == id)
                return a.iban;
        return {};
    }
};

void tst_Payments::validation_rejects_bad_amounts()
{
    world w(this);
    const QString any_valid_iban = make_romanian_iban(QStringLiteral("KMXB"), 1001);
    const auto too_small = w.payments.validate(req(kmx_checking_id, any_valid_iban, 50));
    QVERIFY(!too_small.has_value());
    QCOMPARE(too_small.error().code, Code::amount_too_small);

    const auto too_big = w.payments.validate(req(kmx_checking_id, any_valid_iban, payment_service::max_transfer_minor + 1));
    QVERIFY(!too_big.has_value());
    QCOMPARE(too_big.error().code, Code::amount_too_large);
}

void tst_Payments::validation_rejects_bad_ibans()
{
    world w(this);

    // Corrupt the last digit of an otherwise valid IBAN.
    QString corrupted = make_romanian_iban(QStringLiteral("KMXB"), 1002);
    corrupted[corrupted.size() - 1] = corrupted.at(corrupted.size() - 1) == u'0' ? u'1' : u'0';
    const auto bad_checksum = w.payments.validate(req(kmx_checking_id, corrupted, 5'000));
    QVERIFY(!bad_checksum.has_value());
    QCOMPARE(bad_checksum.error().code, Code::invalid_iban);

    const auto foreign = w.payments.validate(req(kmx_checking_id, QStringLiteral("DE89370400440532013000"), 5'000));
    QVERIFY(!foreign.has_value());
    QCOMPARE(foreign.error().code, Code::invalid_iban);
}

void tst_Payments::insufficient_funds_and_credit_accounts()
{
    world w(this);

    const QString dest = QStringLiteral("RO07TBI1000000000000001");
    // TBI pocket holds only 5.00.
    const auto broke = w.payments.validate(req(tbi_ron_id, dest, 10'000'00));
    QVERIFY(!broke.has_value());
    QCOMPARE(broke.error().code, Code::insufficient_funds);

    // Credit account is already negative: any debit fails.
    const auto credit = w.payments.validate(req(kmx_credit_id, dest, 1'000));
    QVERIFY(!credit.has_value());
    QCOMPARE(credit.error().code, Code::insufficient_funds);
}

void tst_Payments::self_transfer_rejected()
{
    world w(this);
    const QString own_iban = [&]
    {
        for (const auto& a: w.store.accounts())
            if (a.id == tbi_ron_id)
                return a.iban;
        return QString();
    }();

    const auto same = w.payments.validate(req(tbi_ron_id, own_iban, 1'000));
    QVERIFY(!same.has_value());
    QCOMPARE(same.error().code, Code::same_source_and_destination);
}

void tst_Payments::internal_cross_currency_blocked_until_advisor()
{
    world w(this);
    const QString eur_pocket_iban = [&]
    {
        for (const auto& a: w.store.accounts())
            if (a.id == tbi_eur_id)
                return a.iban;
        return QString();
    }();

    const auto cross = w.payments.validate(req(kmx_checking_id, eur_pocket_iban, 2'000));
    QVERIFY(!cross.has_value());
    QCOMPARE(cross.error().code, Code::currency_pair_unsupported);
}

void tst_Payments::internal_execution_moves_balances_and_posts_both_legs()
{
    world w(this);
    QSignalSpy ok_spy(&w.payments, &payment_service::transfer_completed);
    QSignalSpy amended_spy(&w.store, &account_service::ledger_amended);

    const QString dest_iban = [&]
    {
        for (const auto& a: w.store.accounts())
            if (a.id == tbi_ron_id)
                return a.iban;
        return QString();
    }();

    const auto v = w.payments.validate(req(kmx_checking_id, dest_iban, 25'000'00));
    QVERIFY(v.has_value());
    QCOMPARE(v.value().destination_account_id, tbi_ron_id);

    const qint64 src_before = balance_of(w, kmx_checking_id);
    const qint64 dst_before = balance_of(w, tbi_ron_id);

    w.payments.execute(v.value());
    QCOMPARE(ok_spy.count(), 1);

    QCOMPARE(balance_of(w, kmx_checking_id), src_before - 25'000'00);
    QCOMPARE(balance_of(w, tbi_ron_id), dst_before + 25'000'00);

    // Both legs visible in the ledger with mirrored signs.
    int debits = 0, credits = 0;
    for (const auto& t: w.store.transactions())
    {
        if (t.reference.startsWith(QStringLiteral("PM-")))
        {
            if (t.direction == txn_direction::debit)
                ++debits;
            else
                ++credits;
        }
    }
    QCOMPARE(debits, 1);
    QCOMPARE(credits, 1);
    QVERIFY(amended_spy.count() >= 2);

    // Receipt payload sanity.
    const QVariantMap receipt = ok_spy.first().first().toMap();
    QCOMPARE(receipt["internal"].toBool(), true);
    QCOMPARE(receipt["amount_minor"].toLongLong(), qint64(25'000'00));
}

void tst_Payments::external_execution_posts_single_debit()
{
    world w(this);
    QSignalSpy ok_spy(&w.payments, &payment_service::transfer_completed);

    const auto v = w.payments.validate(req(kmx_checking_id, make_romanian_iban(QStringLiteral("BTN1"), 9009), 4'200));
    QVERIFY(v.has_value());
    QCOMPARE(v.value().destination_account_id, -1); // external

    const qint64 before = balance_of(w, kmx_checking_id);
    const int rows_before = w.store.transaction_count();

    w.payments.execute(v.value());
    QCOMPARE(ok_spy.count(), 1);
    QCOMPARE(balance_of(w, kmx_checking_id), before - 4'200);
    QCOMPARE(w.store.transaction_count(), rows_before + 1); // debit only

    const QVariantMap receipt = ok_spy.first().first().toMap();
    QCOMPARE(receipt["internal"].toBool(), false);
    QCOMPARE(receipt["beneficiary_name"].toString(), QStringLiteral("Test beneficiary"));
}

void tst_Payments::scheduled_requires_capability_and_recurs()
{
    world w(this);

    const QString valid_rent_iban = make_romanian_iban(QStringLiteral("BTN1"), 7777);

    // TBI cannot host standing orders (small amount so validation passes first).
    const auto denied = w.payments.schedule(req(tbi_ron_id, valid_rent_iban, 1'00), 1);
    QVERIFY(!denied.has_value());
    QCOMPARE(denied.error().code, Code::capability_missing);

    const auto created = w.payments.schedule(req(kmx_checking_id, valid_rent_iban, 26'000'00), 28);
    QVERIFY(created.has_value());
    QCOMPARE(w.payments.scheduled().size(), 1);
    QVERIFY(w.payments.scheduled().first().next_run > w.clock.now().date());

    // Demo trigger: force-due execution moves money and advances recurrence.
    const qint64 before = balance_of(w, kmx_checking_id);
    w.payments.run_due_now(true);
    QCOMPARE(w.payments.scheduled().size(), 1); // still active
    QCOMPARE(w.payments.scheduled().first().next_run.month(), w.clock.now().date().addMonths(1).month());
    QCOMPARE(balance_of(w, kmx_checking_id), before - 26'000'00);

    // Cancel removes it.
    QVERIFY(w.payments.cancel_scheduled(created.value()));
    QCOMPARE(w.payments.scheduled().size(), 0);
}

QTEST_MAIN(tst_Payments)
#include "tst_payments.moc"
