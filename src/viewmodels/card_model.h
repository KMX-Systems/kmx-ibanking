/// @file src/viewmodels/card_model.h
/// @brief Read/write view over card_service for the Cards page.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QAbstractListModel>
#endif
#include "domain/card.h"
namespace kmx
{

    /// @brief Read/write view over card_service for the Cards page.
    class card_model: public QAbstractListModel
    {
        Q_OBJECT
    public:
        enum roles
        {
            card_id_role = Qt::UserRole + 1,
            account_id_role,
            label_role,
            network_role,
            masked_pan_role,
            full_pan_role,
            cvv_role,
            expiry_role,
            holder_role,
            frozen_role,
            online_payments_role,
            contactless_role,
            is_virtual_role,
            daily_limit_minor_role
        };
        Q_ENUM(roles)

        explicit card_model(class card_service& service, QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

    private:
        card_service& service_;
    };

} // namespace kmx
