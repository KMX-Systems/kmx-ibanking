/// @file tests/tst_fx_service.cpp
/// @brief FX conversion, rate-walk bounds and history tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "services/fx_service.h"
#include <QSignalSpy>
#include <QtTest>

using namespace kmx;

class tst_FxService final: public QObject
{
    Q_OBJECT

private slots:
    void identity_conversion();
    void cross_conversion_near_base();
    void walk_stays_bounded();
    void history_is_capped();
};

void tst_FxService::identity_conversion()
{
    fx_service fx;
    QCOMPARE(fx.convert(12'345, currency_code::ron, currency_code::ron), qint64(12'345));
}

void tst_FxService::cross_conversion_near_base()
{
    fx_service fx;
    // Fresh service sits exactly on base rates.
    const qint64 ron = fx.convert(1'000, currency_code::eur, currency_code::ron);
    QCOMPARE(ron, static_cast<qint64>(std::llround(1'000 * fx_service::base_eur_ron)));

    // Round-trip error stays tiny at demo scale.
    const qint64 back = fx.convert(ron, currency_code::ron, currency_code::eur);
    QVERIFY(qAbs(back - 1'000) <= 2);
}

void tst_FxService::walk_stays_bounded()
{
    fx_service fx;
    QSignalSpy spy(&fx, &fx_service::rates_changed);

    for (int i = 0; i < 300; ++i)
        fx.tick();

    QCOMPARE(spy.count(), 300);

    const double eur_ron = fx.mid(currency_code::eur, currency_code::ron);
    QVERIFY(eur_ron > fx_service::base_eur_ron * 0.97);
    QVERIFY(eur_ron < fx_service::base_eur_ron * 1.03);
}

void tst_FxService::history_is_capped()
{
    fx_service fx;
    for (int i = 0; i < 150; ++i)
        fx.tick();

    const auto series = fx.history(currency_code::eur, currency_code::ron);
    QVERIFY(series.size() <= 90);
    QVERIFY(series.size() >= 90); // we pushed far more than the cap
}

QTEST_MAIN(tst_FxService)
#include "tst_fx_service.moc"
