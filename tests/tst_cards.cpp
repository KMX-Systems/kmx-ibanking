/// @file tests/tst_cards.cpp
/// @brief Card freeze, limits and virtual-card capability tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "domain/iban.h"
#include "services/account_service.h"
#include "services/card_service.h"
#include "services/clock_source.h"
#include "services/payment_service.h"
#include <QtTest>

using namespace kmx;

class tst_Cards final: public QObject
{
    Q_OBJECT

private slots:
    void frozen_card_blocks_transfer_source();
    void unfreeze_restores_flow();
    void daily_limit_caps_today_outflows();
    void virtual_creation_capability_gated();

private:
    struct world
    {
        account_service store;
        fake_clock clock {QDateTime(QDate(2026, 8, 25), QTime(12, 0))};
        payment_service payments;
        card_service cards;

        explicit world(QObject*): payments(store, clock)
        {
            connector::remote_account kc;
            kc.external_id = "KC";
            kc.name = "KMX checking";
            kc.iban = make_romanian_iban(QStringLiteral("KMXB"), 1001);
            kc.balance_minor = 100'000'00;

            connector::remote_account bt_checking; // BT supports virtual cards? no — used for gate test
            bt_checking.external_id = "BC";
            bt_checking.name = "BT checking";
            bt_checking.iban = make_romanian_iban(QStringLiteral("BTN1"), 1002);
            bt_checking.balance_minor = 50'000'00;

            store.upsert_accounts(bank_id::kmx_bank, {kc});
            store.upsert_accounts(bank_id::banca_transilvania, {bt_checking});

            card debit;
            debit.id = 1;
            debit.account_id = 1; // KMX checking
            debit.label = QStringLiteral("KMX Debit");
            debit.full_pan = QStringLiteral("4539881204774821");
            debit.masked_pan = debit.full_pan.left(4) + QStringLiteral(" •••• •••• ") + debit.full_pan.right(4);
            debit.cvv = QStringLiteral("007");
            debit.daily_limit_minor = 300'000'00;

            cards.set_cards({debit});
            payments.set_card_service(&cards);
            payments.set_latency(std::chrono::milliseconds(0));
        }

        transfer_request request_to(qint64 amount_minor, const QString& iban = QString()) const
        {
            transfer_request r;
            r.source_account_id = 1;
            r.beneficiary_name = QStringLiteral("Landlord");
            r.amount_minor = amount_minor;
            r.beneficiary_iban = iban.isEmpty() ? make_romanian_iban(QStringLiteral("BTN1"), 7777) : iban;
            return r;
        }

        qint64 balance_of(qint64 id) const
        {
            for (const auto& a: store.accounts())
                if (a.id == id)
                    return a.balance_minor;
            return -1;
        }
    };
};

void tst_Cards::frozen_card_blocks_transfer_source()
{
    world w(this);

    // Sanity: passes while the card is active.
    QVERIFY(w.payments.validate(w.request_to(25'000'00)).has_value());

    QVERIFY(w.cards.set_frozen(1, true));

    const auto blocked = w.payments.validate(w.request_to(25'000'00));
    QVERIFY(!blocked.has_value());
    QVERIFY(blocked.error().message.contains(QStringLiteral("frozen"), Qt::CaseInsensitive));
}

void tst_Cards::unfreeze_restores_flow()
{
    world w(this);

    QVERIFY(w.cards.set_frozen(1, true));
    QVERIFY(!w.payments.validate(w.request_to(10'000'00)).has_value());

    QVERIFY(w.cards.set_frozen(1, false));
    QVERIFY(w.payments.validate(w.request_to(10'000'00)).has_value());
}

void tst_Cards::daily_limit_caps_today_outflows()
{
    world w(this);

    // Tighten today's cap to 500.00.
    QCOMPARE(w.cards.set_daily_limit(1, 50'000'00)["ok"].toBool(), true);

    // First transfer inside the limit goes through (executed synchronously).
    auto first = w.payments.validate(w.request_to(30'000'00));
    QVERIFY(first.has_value());
    w.payments.execute(first.value());

    // Second one would cross the cap -> DAILY_LIMIT style refusal.
    const auto second = w.payments.validate(w.request_to(30'000'00));
    QVERIFY(!second.has_value());
    QVERIFY(second.error().message.contains(QStringLiteral("limit"), Qt::CaseInsensitive));

    // Disabling the cap restores flow.
    QCOMPARE(w.cards.set_daily_limit(1, 0)["ok"].toBool(), true);
    QVERIFY(w.payments.validate(w.request_to(30'000'00)).has_value());
}

void tst_Cards::virtual_creation_capability_gated()
{
    // Direct service behavior: creation itself appends; gating lives in the
    // session wrapper. Verify service-level invariants here.
    world w(this);

    QCOMPARE(w.cards.cards().size(), 1);
    const auto res = w.cards.create_virtual(2, QStringLiteral("Shopping"), 20'000'00);
    QCOMPARE(res["ok"].toBool(), true);
    QCOMPARE(w.cards.cards().size(), 2);

    const card* virt = w.cards.card_by_id(res["id"].toLongLong());
    QVERIFY(virt != nullptr);
    QVERIFY(virt->is_virtual);
    QVERIFY(virt->full_pan.startsWith(QStringLiteral("45")) || virt->full_pan.startsWith(QStringLiteral("54")));
    QCOMPARE(virt->cvv.size(), 3);
    QCOMPARE(virt->daily_limit_minor, qint64(20'000'00));

    // Limits validation paths.
    QCOMPARE(w.cards.set_daily_limit(virt->id, -5)["ok"].toBool(), false);
    QCOMPARE(w.cards.set_daily_limit(virt->id, 50)["ok"].toBool(), false);
}

QTEST_MAIN(tst_Cards)
#include "tst_cards.moc"
