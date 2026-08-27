/// @file src/services/budget_service.h
/// @brief Monthly envelopes with once-per-month over-budget warnings.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QObject>
    #include <QSet>
    #include <QVariantList>
#endif
#include "domain/budget.h"
#include "domain/currency.h"
#include "services/clock_source.h"
namespace kmx
{

    class account_service;
    class fx_service;
    class notification_service;

    /// @brief Monthly envelopes (plan §Phase 9):
    /// @details spend tracking per category across the aggregated ledger, editable limits, and over-budget warnings that fire exactly once
    /// per category per calendar month.
    class budget_service: public QObject
    {
        Q_OBJECT
    public:
        budget_service(account_service& accounts, fx_service& fx, clock_source& clock, QObject* parent = nullptr);

        void set_notification_service(notification_service* notifications) { notifications_ = notifications; }
        void set_budgets(QVector<budget> budgets) { budgets_ = std::move(budgets); }
        QVector<budget> budgets() const { return budgets_; }

        // {category, limit_minor, spent_minor} for the given month, sorted by
        // utilization desc. RON-normalized.
        Q_INVOKABLE QVariantList budgets_for_month(int year, int month) const;

        // Dashboard widget: current month, top 4 by utilization.
        Q_INVOKABLE QVariantList progress() const;

        Q_INVOKABLE bool set_limit(int category, qint64 limit_minor_ron);
        Q_INVOKABLE qint64 limit_for(int category) const;

        // Called from the session heartbeat; posts over-budget warnings.
        void check_warnings();

    private:
        QByteArray month_key(int category) const; // "YYYY-MM:cat"
        qint64 spent_in_month(int category, const QDateTime& from, const QDateTime& to) const;

        account_service& accounts_;
        fx_service& fx_;
        clock_source& clock_;
        notification_service* notifications_ {nullptr};
        QVector<budget> budgets_;
        QSet<QByteArray> fired_warnings_;
    };

} // namespace kmx
