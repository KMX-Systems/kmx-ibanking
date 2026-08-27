/// @file src/domain/seed_world.h
/// @brief The deterministic demo universe every connector serves slices of.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QVector>
    #include <optional>
#endif
#include "account.h"
#include "bank.h"
#include "beneficiary.h"
#include "budget.h"
#include "card.h"
#include "services/clock_source.h"
#include "transaction.h"
namespace kmx
{

    /// @brief The complete deterministic demo universe.
    /// @details Two calls with the same clock and seed produce identical worlds (ids included) — restarts reset to canon.
    struct seed_world
    {
        QVector<bank_info> banks;
        QVector<account> accounts;
        QVector<transaction> transactions;
        QVector<card> cards;
        QVector<beneficiary> beneficiaries;
        QVector<budget> budgets;

        const account* account_by_id(account_id_t id) const
        {
            for (const auto& a: accounts)
                if (a.id == id)
                    return &a;
            return nullptr;
        }

        const bank_info* bank_by_id(bank_id id) const
        {
            for (const auto& b: banks)
                if (b.id == id)
                    return &b;
            return nullptr;
        }
    };

    inline constexpr quint32 default_world_seed = 0x4B4D5852u; // 'KMXR'

    seed_world generate_seed_world(const clock_source& clock, quint32 seed = default_world_seed);

} // namespace kmx
