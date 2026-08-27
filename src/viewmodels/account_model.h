/// @file src/viewmodels/account_model.h
/// @brief Accounts grouped by bank, with collapsible per-bank headers.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QAbstractListModel>
    #include <QSet>
#endif
#include "domain/bank.h"
#include "domain/currency.h"
namespace kmx
{

    class account_service;
    class fx_service;

    /// @brief Accounts grouped by bank for the Accounts page (plan §Phase 4).
    /// @details row types: bank_header (subtotal) and account_row. Headers are collapsible; collapsing rebuilds the flat row list.
    class account_list_model: public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int display_currency READ display_currency WRITE set_display_currency NOTIFY display_currency_changed)
    public:
        enum roles
        {
            row_type_role = Qt::UserRole + 1,
            bank_id_role,
            account_id_role,
            name_role,
            kind_role,
            currency_code_role,
            iban_role,
            masked_iban_role,
            balance_minor_role, // in the account's own currency
            available_minor_role,
            display_balance_minor_role, // converted to the display currency (headers: subtotal)
            collapsed_role,
            sparkline_role // QVariantList of reals, oldest first
        };
        Q_ENUM(roles)

        enum row_type
        {
            bank_header = 0,
            account_row
        };

        account_list_model(account_service& accounts, fx_service& fx, QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

        int display_currency() const { return display_currency_; }
        void set_display_currency(int currency_index);

        Q_INVOKABLE void toggle_collapsed(int bank_id);
        Q_INVOKABLE void refresh();
        // Named-field accessor for QML delegates (avoids magic role numbers).
        Q_INVOKABLE QVariantMap account_row_at(int row) const;
        Q_INVOKABLE int row_count() const { return rowCount(); }

    signals:
        void display_currency_changed();
        void model_rebuilt();

    private:
        struct entry
        {
            row_type type;
            int bank_id;
            qint64 account_id; // -1 on headers
        };

        void rebuild();
        void rebuild_sparklines();

        account_service& accounts_;
        fx_service& fx_;

        qint64 display_balance(qint64 minor_in_account_currency, currency_code account_currency) const;
        QVector<entry> rows_;
        QSet<int> collapsed_;
        int display_currency_ {0}; // currency_code::ron

        // per-account sparkline cache, rebuilt together with the model
        QHash<qint64, QVariantList> sparklines_;
    };

} // namespace kmx
