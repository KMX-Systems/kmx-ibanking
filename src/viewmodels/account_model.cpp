/// @file src/viewmodels/account_model.cpp
/// @brief Row building, subtotals and reconstructed balance sparklines.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "account_model.h"
#include "domain/account.h"
#include "services/account_service.h"
#include "services/fx_service.h"

namespace kmx
{

    namespace detail
    {
        QString masked_iban(const QString& iban)
        {
            if (iban.size() <= 8)
                return iban;
            return iban.left(4) + QStringLiteral(" •••• •••• ") + iban.right(4);
        }

        // Reconstructs the past from the current balance: balance(t) =
        // currentBalance - Σ(signed amounts posted after t). Deterministic and
        // consistent with the seeded world by construction.
        QVariantList build_sparkline(const QVector<transaction>& txns, qint64 account_id, qint64 current_balance_minor,
                                     const QDateTime& now)
        {
            QVariantList points;
            const int point_count = 7; // six monthly boundaries + today

            qint64 running = current_balance_minor;
            QDateTime cutoff = now;
            points.prepend(static_cast<double>(running));

            for (int i = 0; i < point_count - 1; ++i)
            {
                cutoff = cutoff.addMonths(-1);
                for (const auto& t: txns)
                {
                    if (t.account_id != account_id || t.posted_at <= cutoff)
                        continue;
                    running -= t.signed_amount_minor();
                }
                points.prepend(static_cast<double>(running));
            }
            return points;
        }
    } // namespace detail
    account_list_model::account_list_model(account_service& accounts, fx_service& fx, QObject* parent):
        QAbstractListModel(parent),
        accounts_(accounts),
        fx_(fx)
    {
        refresh();
        connect(&accounts_, &account_service::accounts_changed, this, &account_list_model::refresh);
        // FX ticks only move the display-currency subtotal: avoid a full reset
        // (which would flicker the list and kill scroll positions).
        connect(&fx_, &fx_service::rates_changed, this,
                [this]
                {
                    if (rows_.isEmpty())
                        return;
                    emit dataChanged(index(0, 0), index(static_cast<int>(rows_.size()) - 1, 0), {display_balance_minor_role});
                });
    }

    qint64 account_list_model::display_balance(qint64 minor_in_account_currency, currency_code account_currency) const
    {
        return fx_.convert(minor_in_account_currency, account_currency, static_cast<currency_code>(display_currency_));
    }

    int account_list_model::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    void account_list_model::set_display_currency(int currency_index)
    {
        if (display_currency_ == currency_index)
            return;
        display_currency_ = currency_index;
        emit display_currency_changed();
        refresh();
    }

    QVariant account_list_model::data(const QModelIndex& index, int role) const
    {
        const int row = index.row();
        if (row < 0 || row >= rows_.size())
            return {};

        const entry& r = rows_[row];
        const bank_id bank = static_cast<bank_id>(r.bank_id);

        switch (role)
        {
            case row_type_role:
                return static_cast<int>(r.type);
            case bank_id_role:
                return r.bank_id;
            case collapsed_role:
                return collapsed_.contains(r.bank_id);
        }

        if (r.type == bank_header)
        {
            // Subtotal across the bank's accounts, in display currency.
            qint64 total = 0;
            for (const auto& a: accounts_.accounts())
            {
                if (static_cast<int>(a.bank) != r.bank_id)
                    continue;
                total += display_balance(a.balance_minor, a.currency);
            }
            switch (role)
            {
                case display_balance_minor_role:
                    return total;
                case name_role:
                    return QString::fromLatin1(bank_name(bank));
                default:
                    return {};
            }
        }

        const account* a = nullptr;
        for (const auto& cand: accounts_.accounts())
            if (cand.id == r.account_id)
            {
                a = &cand;
                break;
            }
        if (!a)
            return {};

        switch (role)
        {
            case account_id_role:
                return a->id;
            case name_role:
                return a->name;
            case kind_role:
                return static_cast<int>(a->kind);
            case currency_code_role:
                return QLatin1String(to_code(a->currency));
            case iban_role:
                return a->iban;
            case masked_iban_role:
                return detail::masked_iban(a->iban);
            case balance_minor_role:
                return a->balance_minor;
            case available_minor_role:
                return a->available_minor();
            case sparkline_role:
                return sparklines_.value(a->id);
            default:
                return {};
        }
    }

    QHash<int, QByteArray> account_list_model::roleNames() const
    {
        return {
            {row_type_role, "row_type"},
            {bank_id_role, "bank_id"},
            {account_id_role, "account_id"},
            {name_role, "name"},
            {kind_role, "kind"},
            {currency_code_role, "currency_code"},
            {iban_role, "iban"},
            {masked_iban_role, "masked_iban"},
            {balance_minor_role, "balance_minor"},
            {available_minor_role, "available_minor"},
            {display_balance_minor_role, "display_balance_minor"},
            {collapsed_role, "collapsed"},
            {sparkline_role, "sparkline"},
        };
    }

    QVariantMap account_list_model::account_row_at(int row) const
    {
        if (row < 0 || row >= rows_.size() || rows_[row].type != account_row)
            return {};
        const QModelIndex idx = index(row, 0);
        return {
            {"row_type", static_cast<int>(account_row)},
            {"bank_id", data(idx, bank_id_role)},
            {"account_id", data(idx, account_id_role)},
            {"name", data(idx, name_role)},
            {"kind", data(idx, kind_role)},
            {"currency_code", data(idx, currency_code_role)},
            {"iban", data(idx, iban_role)},
            {"masked_iban", data(idx, masked_iban_role)},
            {"balance_minor", data(idx, balance_minor_role)},
            {"available_minor", data(idx, available_minor_role)},
            {"sparkline", data(idx, sparkline_role)},
        };
    }

    void account_list_model::toggle_collapsed(int bank_id)
    {
        if (collapsed_.contains(bank_id))
            collapsed_.remove(bank_id);
        else
            collapsed_.insert(bank_id);
        refresh();
    }

    void account_list_model::refresh()
    {
        beginResetModel();
        rows_.clear();

        for (int b = 0; b < bank_count; ++b)
        {
            bool has_accounts = false;
            for (const auto& a: accounts_.accounts())
            {
                if (static_cast<int>(a.bank) != b)
                    continue;
                has_accounts = true;
                break;
            }
            if (!has_accounts)
                continue;

            rows_.append({bank_header, b, -1});

            if (!collapsed_.contains(b))
            {
                for (const auto& a: accounts_.accounts())
                {
                    if (static_cast<int>(a.bank) != b)
                        continue;
                    rows_.append({account_row, b, a.id});
                }
            }
        }

        rebuild_sparklines();
        endResetModel();
        emit model_rebuilt();
    }

    void account_list_model::rebuild_sparklines()
    {
        sparklines_.clear();
        const QDateTime now = QDateTime::currentDateTime();
        const auto all = accounts_.transactions();

        for (const auto& a: accounts_.accounts())
            sparklines_.insert(a.id, detail::build_sparkline(all, a.id, a.balance_minor, now));
    }

} // namespace kmx
