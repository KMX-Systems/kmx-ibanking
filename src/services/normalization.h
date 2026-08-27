/// @file src/services/normalization.h
/// @brief Merchant-name cleanup and category inference.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QString>
#endif
#include "domain/transaction.h"
namespace kmx
{

    // Aggregator-side normalization (plan §3): linked banks deliver raw merchant
    // strings; we clean them into display names and infer a spending category.
    // Deterministic, table-driven, unit-tested — the "auto-categorize" promise.

    QString clean_merchant_name(const QString& raw);

    txn_category infer_category_from_merchant(const QString& raw_name);
    bool merchant_looks_known(const QString& raw_name);

    // English display label per category id (CSV + debug surfaces).
    const char* category_label(txn_category category);

} // namespace kmx
