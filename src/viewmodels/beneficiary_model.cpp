/// @file src/viewmodels/beneficiary_model.cpp
/// @brief Beneficiary CRUD, favourites and last-used stamping.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "beneficiary_model.h"
#include <algorithm>

namespace kmx
{

    beneficiary_model::beneficiary_model(QObject* parent): QAbstractListModel(parent)
    {
    }

    int beneficiary_model::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    QVariant beneficiary_model::data(const QModelIndex& index, int role) const
    {
        const int row = index.row();
        if (row < 0 || row >= items_.size())
            return {};

        const auto& b = items_[row];
        switch (role)
        {
            case beneficiary_id_role:
                return b.id;
            case name_role:
                return b.name;
            case iban_role:
                return b.iban;
            case currency_code_role:
                return QLatin1String(to_code(b.default_currency));
            case favorite_role:
                return b.favorite;
            case last_used_at_role:
                return b.last_used_at;
            default:
                return {};
        }
    }

    QHash<int, QByteArray> beneficiary_model::roleNames() const
    {
        return {
            {beneficiary_id_role, "beneficiary_id"}, {name_role, "name"},         {iban_role, "iban"},
            {currency_code_role, "currency_code"},   {favorite_role, "favorite"}, {last_used_at_role, "last_used_at"},
        };
    }

    void beneficiary_model::set_beneficiaries(QVector<beneficiary> list)
    {
        beginResetModel();
        items_ = std::move(list);
        sort_favorites_first();
        endResetModel();
        emit changed();
    }

    void beneficiary_model::sort_favorites_first()
    {
        std::stable_sort(items_.begin(), items_.end(), [](const beneficiary& a, const beneficiary& b) { return a.favorite > b.favorite; });
    }

    void beneficiary_model::add(const QString& name, const QString& iban, int currency_index)
    {
        qint64 next_id = 1;
        for (const auto& b2: items_)
            next_id = std::max(next_id, b2.id + 1);

        beneficiary b;
        b.id = next_id;
        b.name = name.simplified();
        b.iban = iban;
        b.default_currency = static_cast<currency_code>(std::clamp(currency_index, 0, 2));

        const int insertRow = 0; // new entries land on top
        beginInsertRows({}, insertRow, insertRow);
        items_.insert(insertRow, b);
        endInsertRows();
        emit changed();
    }

    bool beneficiary_model::remove(int row)
    {
        if (row < 0 || row >= items_.size())
            return false;
        beginRemoveRows({}, row, row);
        items_.remove(row);
        endRemoveRows();
        emit changed();
        return true;
    }

    void beneficiary_model::toggle_favorite(int row)
    {
        if (row < 0 || row >= items_.size())
            return;
        items_[row].favorite = !items_[row].favorite;

        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {favorite_role});
        sort_favorites_first();
        // Full refresh keeps indices honest after reordering.
        beginResetModel();
        endResetModel();
        emit changed();
    }

    QVariantMap beneficiary_model::get(int row) const
    {
        if (row < 0 || row >= items_.size())
            return {};
        const auto& b = items_[row];
        return {{"beneficiary_id", b.id},
                {"name", b.name},
                {"iban", b.iban},
                {"currency_code", QLatin1String(to_code(b.default_currency))},
                {"favorite", b.favorite}};
    }

    void beneficiary_model::mark_used_by_iban(const QString& iban)
    {
        for (int r = 0; r < items_.size(); ++r)
        {
            if (QString::compare(items_[r].iban, iban, Qt::CaseInsensitive) == 0)
            {
                items_[r].last_used_at = QDateTime::currentDateTime();
                const QModelIndex idx = index(r, 0);
                emit dataChanged(idx, idx, {last_used_at_role});
                return;
            }
        }
    }

} // namespace kmx
