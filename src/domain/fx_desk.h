/// @file src/domain/fx_desk.h
/// @brief Per-bank FX pricing: spreads and fees for a directed currency pair.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QVector>
    #include <QtGlobal>
    #include <optional>
#endif
#include "bank.h"
#include "currency.h"
namespace kmx
{

    /// @brief Pricing rule for one directed currency pair at one bank's exchange desk.
    /// @details Effective rate = mid rate * (1 - spread_bps/10'000); fees apply afterwards.
    struct fx_desk_pair_rule
    {
        currency_code from {currency_code::ron};
        currency_code to {currency_code::eur};
        qint16 spread_bps {0};       // basis points, e.g. 8 = 0.08%
        qint16 fee_bps {0};          // percentage fee in bps
        qint64 fee_fixed_minor {0};  // fixed fee in TARGET currency minor units
        qint64 min_ticket_minor {0}; // minimum source amount accepted
        qint64 max_ticket_minor {0}; // 0 = unlimited
    };

    /// @brief Per-bank FX desks — the raw material of the exchange advisor.
    /// @details Roster pricing per plan §2: KMX: all pairs 15 bps spread, no fee BT: RON<->* 55 bps + 800 minor (8 RON) fixed TBI: all
    /// pairs 8 bps, no fee <- usually the advisor's winner Erste: EUR<->* 20 bps; other pairs 50 bps + 500 minor (5 RON)
    class fx_desk
    {
    public:
        fx_desk() = default;
        explicit fx_desk(bank_id bank): bank_(bank) {}

        void add_rule(fx_desk_pair_rule rule) { rules_.append(rule); }

        std::optional<fx_desk_pair_rule> rule_for(currency_code from, currency_code to) const
        {
            for (const auto& r: rules_)
                if (r.from == from && r.to == to)
                    return r;
            return std::nullopt;
        }

        bank_id bank() const { return bank_; }
        const QVector<fx_desk_pair_rule>& rules() const { return rules_; }

    private:
        bank_id bank_ {bank_id::kmx_bank};
        QVector<fx_desk_pair_rule> rules_;
    };

} // namespace kmx
