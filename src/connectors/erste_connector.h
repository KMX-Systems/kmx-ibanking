/// @file src/connectors/erste_connector.h
/// @brief Erste Bank persona: EUR specialist with a rate-limited data API.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#include "seed_bank_connector.h"
#include "services/clock_source.h"

namespace kmx
{

    /// @brief Erste Bank persona (plan §2):
    /// @details EUR pricing specialist whose data API rate-limits refreshes to one attempt per cooldown window.
    class erste_connector final: public seed_bank_connector
    {
    public:
        erste_connector(const seed_world& world, clock_source& clock):
            seed_bank_connector(bank_id::erste_bank, world,
                                {std::chrono::minutes(8 * 60),
                                 /*booked_only*/ false,
                                 /*rawStrings*/ false,
                                 /*sparse*/ false, options::category_quality::native},
                                clock),
            clock_(clock)
        {
        }

        // Demo-tunable cooldown; production default is 5 minutes.
        void set_rate_limit(std::chrono::milliseconds cooldown) { cooldown_ = cooldown; }

        fx_desk desk() const override
        {
            // EUR<->* at 20 bps; the rest at 50 bps + 5 RON fixed fee.
            fx_desk desk(bank());
            const auto add = [&](currency_code a, currency_code b, qint16 bps, qint64 fee)
            { desk.add_rule({a, b, bps, 0, fee, 1'000, 0}); };
            add(currency_code::eur, currency_code::ron, 20, 0);
            add(currency_code::ron, currency_code::eur, 20, 0);
            add(currency_code::eur, currency_code::usd, 20, 0);
            add(currency_code::usd, currency_code::eur, 20, 0);
            add(currency_code::ron, currency_code::usd, 50, 500);
            add(currency_code::usd, currency_code::ron, 50, 500);
            return desk;
        }

    protected:
        std::optional<connector::sync_error> pre_fetch_guard() override
        {
            const auto now_ms = clock_.now().toMSecsSinceEpoch();
            if (last_attempt_ms_ > 0 && now_ms - last_attempt_ms_ < static_cast<qint64>(cooldown_.count()))
            {
                const qint64 remaining = static_cast<qint64>(cooldown_.count()) - (now_ms - last_attempt_ms_);
                return connector::make_error(connector::sync_error_code::rate_limited, QStringLiteral("Data API cooling down"),
                                             std::chrono::milliseconds(remaining));
            }
            last_attempt_ms_ = now_ms;
            return std::nullopt;
        }

    private:
        clock_source& clock_;
        std::chrono::milliseconds cooldown_ {std::chrono::minutes(5)};
        qint64 last_attempt_ms_ {0};
    };

} // namespace kmx
