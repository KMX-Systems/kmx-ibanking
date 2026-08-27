/// @file tests/tst_seed_generator.cpp
/// @brief Determinism and shape tests for the seeded world.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "domain/iban.h"
#include "domain/seed_world.h"
#include "services/clock_source.h"
#include <QHash>
#include <QSet>
#include <QtTest>
#include <algorithm>

using namespace kmx;

class tst_SeedGenerator final: public QObject
{
    Q_OBJECT

private slots:
    void world_is_deterministic();
    void world_has_expected_shape();
    void all_ibans_pass_mod97();
    void transactions_stay_in_the_past();
};

static qint64 world_checksum(const seed_world& w)
{
    qint64 sum = 0;
    for (const auto& t: w.transactions)
        sum += t.id * 31 + t.amount_minor * 17 + t.posted_at.toSecsSinceEpoch() + qHash(t.counterparty);
    for (const auto& a: w.accounts)
        sum += a.id * 7 + a.balance_minor;
    return sum;
}

void tst_SeedGenerator::world_is_deterministic()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(15, 30, 0)));

    const seed_world first = generate_seed_world(clock);
    const seed_world second = generate_seed_world(clock);

    QCOMPARE(first.transactions.size(), second.transactions.size());
    QCOMPARE(world_checksum(first), world_checksum(second));

    // Stable ids across generations.
    if (!first.transactions.isEmpty())
        QCOMPARE(first.transactions.first().id, second.transactions.first().id);
}

void tst_SeedGenerator::world_has_expected_shape()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(15, 30, 0)));
    const seed_world w = generate_seed_world(clock);

    QCOMPARE(w.banks.size(), bank_count);
    QCOMPARE(w.accounts.size(), 8);
    QCOMPARE(w.cards.size(), 4);
    QCOMPARE(w.beneficiaries.size(), 6);
    QCOMPARE(w.budgets.size(), 5);

    // Plan volume target: ~12 months across eight accounts.
    QVERIFY2(w.transactions.size() >= 800, "too few transactions");
    QVERIFY2(w.transactions.size() <= 4000, "too many transactions");

    // Ids are unique and sequential from 1.
    QSet<qint64> ids;
    for (const auto& t: w.transactions)
        ids.insert(t.id);
    QCOMPARE(ids.size(), w.transactions.size());
    QCOMPARE(*std::min_element(ids.cbegin(), ids.cend()), qint64(1));
    QCOMPARE(*std::max_element(ids.cbegin(), ids.cend()), qint64(w.transactions.size()));

    // Every transaction points at an existing account; every account at a bank.
    for (const auto& t: w.transactions)
        QVERIFY(w.account_by_id(t.account_id) != nullptr);
    for (const auto& a: w.accounts)
        QVERIFY(a.bank >= bank_id::kmx_bank && a.bank <= bank_id::erste_bank);
}

void tst_SeedGenerator::all_ibans_pass_mod97()
{
    // Every generated IBAN must satisfy ISO 7064 MOD-97-10.
    const auto all_accounts_valid = [this](const seed_world& w)
    {
        for (const auto& a: w.accounts)
            if (!is_valid_iban(a.iban))
                return false;
        return true;
    };

    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(15, 30, 0)));
    QVERIFY(all_accounts_valid(generate_seed_world(clock)));
    QVERIFY(is_valid_iban(make_romanian_iban(QStringLiteral("ERS1"), 7007)));

    // Tampered check digits must fail.
    const QString good = make_romanian_iban(QStringLiteral("TBI1"), 2002);
    QString bad = good;
    bad[3] = bad.at(3) == u'9' ? u'8' : u'9';
    QVERIFY(!is_valid_iban(bad));
    QVERIFY(!is_valid_iban(QStringLiteral("RO00SHORT")));
}

void tst_SeedGenerator::transactions_stay_in_the_past()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(15, 30, 0)));
    const seed_world w = generate_seed_world(clock);

    for (const auto& t: w.transactions)
    {
        QVERIFY2(t.posted_at <= clock.now(),
                 qPrintable(QStringLiteral("txn %1 (%2) is in the future: %3").arg(t.id).arg(t.counterparty, t.posted_at.toString())));
    }
}

QTEST_MAIN(tst_SeedGenerator)
#include "tst_seed_generator.moc"
