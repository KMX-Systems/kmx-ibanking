/// @file src/domain/capabilities.h
/// @brief Feature matrix each bank connector advertises.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QtGlobal>
#endif
namespace kmx
{

    /// @brief Feature matrix exposed by each bank connector (plan §2).
    struct bank_capabilities
    {
        bool pending_txns {false};
        bool auto_categories {false};
        bool virtual_cards {false};
        bool scheduled_payments {false};
    };

} // namespace kmx
