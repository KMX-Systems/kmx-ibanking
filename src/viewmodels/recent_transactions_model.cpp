/// @file src/viewmodels/recent_transactions_model.cpp
/// @brief Feed rows, filtering by account and reload on amendment.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "recent_transactions_model.h"
#include "services/account_service.h"

namespace kmx
{

    recent_transactions_model::recent_transactions_model(account_service& accounts, QObject* parent):
        QAbstractListModel(parent),
        accounts_(accounts)
    {
        reload();
        connect(&accounts_, &account_service::transactions_merged, this, [this](int, int) { reload(); });
        // Manual recategorization/notes amend rows in place: cheap full reload.
        connect(&accounts_, &account_service::ledger_amended, this, [this] { reload(); });
    }

    int recent_transactions_model::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    QVariant recent_transactions_model::data(const QModelIndex& index, int role) const
    {
        const int row = index.row();
        if (row < 0 || row >= rows_.size())
            return {};

        const transaction& t = rows_[row];

        switch (role)
        {
            case counterparty_role:
                return t.counterparty;
            case signed_amount_minor_role:
                return t.signed_amount_minor();
            case currency_code_role:
                return QLatin1String(to_code(t.currency));
            case posted_at_role:
                return t.posted_at;
            case category_role:
                return static_cast<int>(t.category);
            case category_source_role:
                return static_cast<int>(t.category_source);
            case status_role:
                return static_cast<int>(t.status);
            case transaction_id_role:
                return t.id;
            case reference_role:
                return t.reference;
            case direction_role:
                return static_cast<int>(t.direction);
            case counterparty_iban_role:
                return t.counterparty_iban;
            case fx_note_role:
                return t.fx_note;
            case note_role:
                return t.note;
            case bank_id_role:
            {
                // Resolve the owning account's bank (accounts are few — linear scan).
                for (const auto& a: accounts_.accounts())
                    if (a.id == t.account_id)
                        return static_cast<int>(a.bank);
                return -1;
            }
            default:
                return {};
        }
    }

    QHash<int, QByteArray> recent_transactions_model::roleNames() const
    {
        return {
            {counterparty_role, "counterparty"},
            {signed_amount_minor_role, "signed_amount_minor"},
            {currency_code_role, "currency_code"},
            {posted_at_role, "posted_at"},
            {category_role, "category"},
            {category_source_role, "category_source"},
            {status_role, "status"},
            {bank_id_role, "bank_id"},
            {transaction_id_role, "transaction_id"},
            {reference_role, "reference"},
            {direction_role, "direction"},
            {counterparty_iban_role, "counterparty_iban"},
            {fx_note_role, "fx_note"},
            {note_role, "note"},
        };
    }

    void recent_transactions_model::set_limit(int limit)
    {
        if (limit_ == limit)
            return;
        limit_ = limit;
        emit limit_changed();
        reload();
    }

    void recent_transactions_model::set_account_filter(qint64 account_id)
    {
        if (account_filter_ == account_id)
            return;
        account_filter_ = account_id;
        emit account_filter_changed();
        reload();
    }

    void recent_transactions_model::reload()
    {
        beginResetModel();
        rows_.clear();

        for (const auto& t: accounts_.transactions())
        {
            if (account_filter_ >= 0 && t.account_id != account_filter_)
                continue;
            rows_.append(t);
            if (limit_ > 0 && static_cast<int>(rows_.size()) >= limit_)
                break;
        }
        endResetModel();
    }

} // namespace kmx
