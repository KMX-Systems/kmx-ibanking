/// @file src/services/fx_service.h
/// @brief Mid-market FX rates used for display conversions.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QHash>
    #include <QObject>
    #include <QVector>
#endif
#include "domain/currency.h"
namespace kmx
{

    /// @brief Mid-market FX for display conversions (plan §FX spec).
    /// @details Trading quotes with spreads/fees arrive with the exchange advisor (P7); everything before that converts through the clean
    /// mid rate here. Rates walk slightly on every tick so sparklines and "indicative" totals feel alive. Walks are cosmetic — never used
    /// for money movement pricing decisions.
    class fx_service: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int tick_count READ tick_count NOTIFY ticked)
    public:
        explicit fx_service(QObject* parent = nullptr);

        // Base mid rates around mid-2026 levels.
        // Triangle consistency matters: USDRON derives from the other two so
        // RON->USD->EUR returns the same mid as RON->EUR — spreads, not table
        // gaps, must be what drives routing decisions.
        static constexpr double base_eur_ron = 4.9723;
        static constexpr double base_eur_usd = 1.0842;
        static constexpr double base_usd_ron = base_eur_ron / base_eur_usd;

        double mid(currency_code from, currency_code to) const;

        // Minor-unit conversion through the mid rate (half-away-from-zero).
        qint64 convert(qint64 minor, currency_code from, currency_code to) const;

        // Cosmetics: advance the walk one step (bounded drift vs base ±3%).
        void tick();
        Q_INVOKABLE void shock(); // demo scenario: sudden divergence between desks

        int tick_count() const { return ticks_; }

        // Last N observed mids for a pair (oldest first); feeds sparklines (P7+).
        QVector<double> history(currency_code from, currency_code to) const;

    signals:
        void ticked();
        void rates_changed();

    private:
        static int pair_key(currency_code from, currency_code to) { return static_cast<int>(from) * currency_count + static_cast<int>(to); }

        void set_base(currency_code from, currency_code to, double mid);
        void record_history();

        QHash<int, double> mid_;
        QHash<int, double> base_;
        QHash<int, QVector<double>> history_;
        int ticks_ {0};
    };

} // namespace kmx
