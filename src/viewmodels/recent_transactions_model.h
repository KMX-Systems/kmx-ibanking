/// @file src/viewmodels/recent_transactions_model.h
/// @brief Recent-activity feed, also serving per-account statements.
/// 4).
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QAbstractListModel>
#endif
#include "domain/bank.h"
#include "domain/transaction.h"
namespace kmx
{

    class account_service;

    /// @brief Cross-bank recent-activity feed for the dashboard; also serves the per-account statement view via `account_filter` (plan
    /// §Phase 4).
    class recent_transactions_model: public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int limit READ limit WRITE set_limit NOTIFY limit_changed)
        Q_PROPERTY(qint64 account_filter READ account_filter WRITE set_account_filter NOTIFY account_filter_changed)
    public:
        enum roles
        {
            counterparty_role = Qt::UserRole + 1,
            signed_amount_minor_role,
            currency_code_role,
            posted_at_role,
            category_role,
            category_source_role,
            status_role,
            bank_id_role, // resolved through the owning account
            transaction_id_role,
            reference_role, // composite external identity ("bank:ext-id")
            direction_role,
            counterparty_iban_role,
            fx_note_role,
            note_role
        };
        Q_ENUM(roles)

        explicit recent_transactions_model(account_service& accounts, QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

        int limit() const { return limit_; }
        void set_limit(int limit);

        QVector<transaction> transactions() const { return rows_; }

        qint64 account_filter() const { return account_filter_; }
        void set_account_filter(qint64 account_id); // -1 = all accounts

    signals:
        void limit_changed();
        void account_filter_changed();

    private:
        void reload();

        account_service& accounts_;
        QVector<transaction> rows_;
        int limit_ {8};
        qint64 account_filter_ {-1};
    };

} // namespace kmx
