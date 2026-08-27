/// @file src/services/analytics_service.h
/// @brief Read-only aggregations over the ledger, normalized to RON.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QObject>
    #include <QVariantList>
    #include <QVector>
#endif
#include "domain/bank.h"
#include "domain/currency.h"
#include "services/clock_source.h"
namespace kmx
{

    class account_service;
    class fx_service;

    /// @brief Read-only aggregations over the unified ledger (plan §Phase 9).
    /// @details All money is normalized into RON for cross-currency comparability — the same convention as budgets.
    class analytics_service: public QObject
    {
        Q_OBJECT
    public:
        explicit analytics_service(account_service& accounts, fx_service& fx, clock_source& clock, QObject* parent = nullptr);

        // Last `months` months, oldest first: {label, income_minor, expense_minor} in
        // RON minor units. Current month included as the last entry.
        Q_INVOKABLE QVariantList month_cashflow(int months) const;

        // Category spending for a calendar month: {category, spent_minor} in ron,
        // descending by amount. Debits only; pending rows included (real banks do).
        Q_INVOKABLE QVariantList category_breakdown(int year, int month) const;

        // Net worth reconstruction at monthly boundaries, oldest first:
        // {label, total_minor} in the display currency. Last point == live net worth.
        Q_INVOKABLE QVariantList net_worth_series(int points) const;

        static QString month_label(int year, int month);

    private:
        QDateTime month_start(int year, int month) const;

        account_service& accounts_;
        fx_service& fx_;
        clock_source& clock_;
    };

} // namespace kmx
