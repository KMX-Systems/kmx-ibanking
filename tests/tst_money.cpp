/// @file tests/tst_money.cpp
/// @brief Money arithmetic, conversion and rounding tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "domain/money.h"
#include <QtTest>

using namespace kmx;

class tst_Money final: public QObject
{
    Q_OBJECT

private slots:
    void from_major_is_exact();
    void arithmetic_requires_same_currency();
    void convert_rounds_half_away_from_zero();
    void negative_amounts_convert_symmetrically();
    void inverse_rate_round_trips();
};

void tst_Money::from_major_is_exact()
{
    const money m = money::from_major(1425, currency_code::ron); // 1 425,00 RON
    QCOMPARE(m.minor(), qint64(142'500));
    QCOMPARE(m.currency(), currency_code::ron);
}

void tst_Money::arithmetic_requires_same_currency()
{
    const money a = money::from_major(10, currency_code::eur);
    const money b = money::from_major(2, currency_code::eur);

    QCOMPARE((a + b).minor(), qint64(1'200));
    QCOMPARE((a - b).minor(), qint64(800));
    QCOMPARE((-a).minor(), qint64(-1'000));
    QVERIFY(a.is_same_currency(b));
}

void tst_Money::convert_rounds_half_away_from_zero()
{
    // eur->ron mid 4.9723 => rate_micro = 4'972'300.
    const qint64 eur_ron = 4'972'300;

    // 100.01 EUR * 4.9723 = 497.279723 RON -> rounds to 497.28 (half away).
    QCOMPARE(money::convert(money(10'001, currency_code::eur), currency_code::ron, eur_ron).minor(), qint64(49'728));

    // Exact division stays exact.
    QCOMPARE(money::convert(money(10'000, currency_code::eur), currency_code::ron, eur_ron).minor(), qint64(49'723));

    // .5 minor boundary rounds AWAY from zero.
    QCOMPARE(money::convert(money(1, currency_code::ron), currency_code::ron, 2'500'000).minor(),
             qint64(3)); // 0.01 * 2.5 = 0.025 -> 0.03
}

void tst_Money::negative_amounts_convert_symmetrically()
{
    const qint64 eur_ron = 4'972'300;
    const money positive = money::convert(money(10'001, currency_code::eur), currency_code::ron, eur_ron);
    const money negative = money::convert(money(-10'001, currency_code::eur), currency_code::ron, eur_ron);
    QCOMPARE(negative.minor(), -positive.minor());
}

void tst_Money::inverse_rate_round_trips()
{
    const qint64 eur_ron = 4'972'300;
    const qint64 ron_eur = money::inverse_rate_micro(eur_ron);
    QVERIFY(ron_eur > 0);

    // Round trip error stays under one cent on realistic amounts.
    const money original(123'456, currency_code::eur);
    const money to_ron = money::convert(original, currency_code::ron, eur_ron);
    const money back = money::convert(to_ron, currency_code::eur, ron_eur);
    QVERIFY(qAbs(back.minor() - original.minor()) <= 1);
}

QTEST_MAIN(tst_Money)
#include "tst_money.moc"
