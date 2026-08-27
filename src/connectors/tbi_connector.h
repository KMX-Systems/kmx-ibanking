/// @file src/connectors/tbi_connector.h
/// @brief TBI Bank persona: challenger with sparse metadata and the cheapest desk.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#include "seed_bank_connector.h"

namespace kmx
{

    /// @brief TBI bank persona (plan §2):
    /// @details challenger — instant sync, sparse metadata, partial native categories, cheapest FX desk in the roster.
    class tbi_connector final: public seed_bank_connector
    {
    public:
        explicit tbi_connector(const seed_world& world, clock_source& clock):
            seed_bank_connector(bank_id::tbi_bank, world,
                                {std::chrono::minutes(60),
                                 /*booked_only*/ false,
                                 /*rawStrings*/ false,
                                 /*sparse*/ true, options::category_quality::partial},
                                clock)
        {
        }

        bank_capabilities capabilities() const override
        {
            bank_capabilities c = seed_bank_connector::capabilities();
            c.virtual_cards = true; // TBI issues virtual cards too
            return c;
        }

        fx_desk desk() const override
        {
            // All pairs at 8 bps, no fee — usually the advisor's winner.
            fx_desk desk(bank());
            for (auto [a, b]: {std::pair {currency_code::ron, currency_code::eur},
                               {currency_code::eur, currency_code::ron},
                               {currency_code::ron, currency_code::usd},
                               {currency_code::usd, currency_code::ron},
                               {currency_code::eur, currency_code::usd},
                               {currency_code::usd, currency_code::eur}})
                desk.add_rule({a, b, 8, 0, 0, 1'000, 0});
            return desk;
        }
    };

} // namespace kmx
