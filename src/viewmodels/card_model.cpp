/// @file src/viewmodels/card_model.cpp
/// @brief Card row exposure and mutation forwarding.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "card_model.h"
#include "services/card_service.h"

namespace kmx
{

    card_model::card_model(card_service& service, QObject* parent): QAbstractListModel(parent), service_(service)
    {
        connect(&service_, &card_service::cards_changed, this,
                [this]
                {
                    beginResetModel();
                    endResetModel();
                });
    }

    int card_model::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(service_.cards().size());
    }

    QVariant card_model::data(const QModelIndex& index, int role) const
    {
        const int row = index.row();
        const auto& cards = service_.cards();
        if (row < 0 || row >= cards.size())
            return {};

        const card& c = cards[row];
        switch (role)
        {
            case card_id_role:
                return c.id;
            case account_id_role:
                return c.account_id;
            case label_role:
                return c.label;
            case network_role:
                return static_cast<int>(c.network);
            case masked_pan_role:
                return c.masked_pan;
            case full_pan_role:
                return c.full_pan;
            case cvv_role:
                return c.cvv;
            case expiry_role:
                return c.expiry_mmyy;
            case holder_role:
                return c.holder_name;
            case frozen_role:
                return c.frozen;
            case online_payments_role:
                return c.online_payments;
            case contactless_role:
                return c.contactless;
            case is_virtual_role:
                return c.is_virtual;
            case daily_limit_minor_role:
                return c.daily_limit_minor;
            default:
                return {};
        }
    }

    QHash<int, QByteArray> card_model::roleNames() const
    {
        return {
            {card_id_role, "card_id"},
            {account_id_role, "account_id"},
            {label_role, "label"},
            {network_role, "network"},
            {masked_pan_role, "masked_pan"},
            {full_pan_role, "full_pan"},
            {cvv_role, "cvv"},
            {expiry_role, "expiry"},
            {holder_role, "holder"},
            {frozen_role, "frozen"},
            {online_payments_role, "online_payments"},
            {contactless_role, "contactless"},
            {is_virtual_role, "is_virtual"},
            {daily_limit_minor_role, "daily_limit_minor"},
        };
    }

} // namespace kmx
