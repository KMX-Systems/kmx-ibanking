/// @file src/services/fx_service.cpp
/// @brief Bounded cosmetic rate walks and conversion helpers.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "fx_service.h"
#include "domain/money.h"
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

namespace kmx
{

    namespace detail
    {
        constexpr int history_cap = 90;
        constexpr double max_drift_from_base = 0.03; // ±3% vs base, keeps walks sane
        constexpr double step_sigma = 0.0015;        // ±0.15% per tick
    } // namespace detail
    fx_service::fx_service(QObject* parent): QObject(parent)
    {
        set_base(currency_code::eur, currency_code::ron, base_eur_ron);
        set_base(currency_code::ron, currency_code::eur, 1.0 / base_eur_ron);
        set_base(currency_code::usd, currency_code::ron, base_usd_ron);
        set_base(currency_code::ron, currency_code::usd, 1.0 / base_usd_ron);
        set_base(currency_code::eur, currency_code::usd, base_eur_usd);
        set_base(currency_code::usd, currency_code::eur, 1.0 / base_eur_usd);

        // Identity pairs convert trivially.
        for (int i = 0; i < currency_count; ++i)
        {
            mid_.insert(pair_key(static_cast<currency_code>(i), static_cast<currency_code>(i)), 1.0);
            base_.insert(pair_key(static_cast<currency_code>(i), static_cast<currency_code>(i)), 1.0);
        }

        record_history();
    }

    void fx_service::set_base(currency_code from, currency_code to, double mid)
    {
        mid_.insert(pair_key(from, to), mid);
        base_.insert(pair_key(from, to), mid);
    }

    double fx_service::mid(currency_code from, currency_code to) const
    {
        return mid_.value(pair_key(from, to), 1.0);
    }

    qint64 fx_service::convert(qint64 minor, currency_code from, currency_code to) const
    {
        if (from == to)
            return minor;
        const qint64 rate_micro = static_cast<qint64>(std::llround(mid(from, to) * 1'000'000.0));
        return money::convert(money(minor, from), to, rate_micro).minor();
    }

    void fx_service::tick()
    {
        auto* rng = QRandomGenerator::global();

        QHashIterator<int, double> it(mid_);
        while (it.hasNext())
        {
            it.next();
            const int key = it.key();
            if (key / currency_count == key % currency_count)
                continue; // identity pairs don't move

            const double base = base_.value(key, 1.0);
            double value = it.value();

            // Box-Muller-ish: average of uniforms is plenty for a demo walk.
            const double shock = (rng->generateDouble() + rng->generateDouble() - 1.0) * detail::step_sigma;
            value *= (1.0 + shock);

            if (std::fabs(value - base) / base > detail::max_drift_from_base)
                value = base * (value > base ? 1.0 + detail::max_drift_from_base : 1.0 - detail::max_drift_from_base);

            mid_.insert(key, value);
        }

        ++ticks_;
        record_history();
        emit ticked();
        emit rates_changed();
    }

    void fx_service::shock()
    {
        // One-off ±1.5% jolt per non-identity pair: recommendations visibly change.
        auto* rng = QRandomGenerator::global();
        QHashIterator<int, double> it(mid_);
        while (it.hasNext())
        {
            it.next();
            const int key = it.key();
            if (key / currency_count == key % currency_count)
                continue;
            const double dir = rng->bounded(2) == 0 ? 1.0 : -1.0;
            mid_.insert(key, it.value() * (1.0 + dir * 0.015));
        }
        record_history();
        emit ticked();
        emit rates_changed();
    }

    QVector<double> fx_service::history(currency_code from, currency_code to) const
    {
        return history_.value(pair_key(from, to));
    }

    void fx_service::record_history()
    {
        QHashIterator<int, double> it(mid_);
        while (it.hasNext())
        {
            it.next();
            if (it.key() / currency_count == it.key() % currency_count)
                continue; // identity pairs have nothing interesting to chart
            auto& series = history_[it.key()];
            series.append(it.value());
            if (series.size() > detail::history_cap)
                series.remove(0, series.size() - detail::history_cap);
        }
    }

} // namespace kmx
