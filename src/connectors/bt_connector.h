/// @file src/connectors/bt_connector.h
/// @brief Banca Transilvania persona: slow batch sync, raw merchant strings.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#include "seed_bank_connector.h"

namespace kmx
{

    /// @brief Banca Transilvania persona (plan §2):
    /// @details slow batch sync, booked-only transactions with raw merchant strings, session expires every 15 minutes.
    class bt_connector final: public seed_bank_connector
    {
    public:
        explicit bt_connector(const seed_world& world, clock_source& clock):
            seed_bank_connector(bank_id::banca_transilvania, world,
                                {/*validity*/ std::chrono::minutes(15),
                                 /*booked_only*/ true,
                                 /*rawStrings*/ true,
                                 /*sparse*/ false, options::category_quality::inferred},
                                clock)
        {
        }

        fx_desk desk() const override
        {
            // RON<->* at 55 bps + 8 RON fixed fee.
            fx_desk desk(bank());
            const auto add = [&](currency_code a, currency_code b) { desk.add_rule({a, b, 55, 0, 800, 5'000, 0}); };
            add(currency_code::ron, currency_code::eur);
            add(currency_code::ron, currency_code::usd);
            add(currency_code::eur, currency_code::ron);
            add(currency_code::usd, currency_code::ron);
            return desk;
        }
    };

} // namespace kmx
