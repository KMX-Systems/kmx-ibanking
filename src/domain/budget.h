/// @file src/domain/budget.h
/// @brief Monthly spending envelope, normalized to the display currency.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaType>
    #include <QtGlobal>
#endif
#include "currency.h"
#include "transaction.h"
namespace kmx
{

    /// @brief Monthly spending envelope.
    /// @details Limits are expressed in the display currency (RON for this demo) so budgets stay comparable across banks and accounts.
    struct budget
    {
        txn_category category {txn_category::other};
        qint64 monthly_limit_minor {0};
        currency_code limit_currency {currency_code::ron};
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::budget)
