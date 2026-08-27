/// @file src/viewmodels/beneficiary_model.h
/// @brief Beneficiary directory for the transfer wizard, favourites first.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QAbstractListModel>
#endif
#include "domain/beneficiary.h"
namespace kmx
{

    /// @brief beneficiary directory for the transfer wizard (plan §Phase 6):
    /// @details favorites first, CRUD from the manage screen.
    class beneficiary_model: public QAbstractListModel
    {
        Q_OBJECT
    public:
        enum roles
        {
            beneficiary_id_role = Qt::UserRole + 1,
            name_role,
            iban_role,
            currency_code_role,
            favorite_role,
            last_used_at_role
        };
        Q_ENUM(roles)

        explicit beneficiary_model(QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

        void set_beneficiaries(QVector<beneficiary> list);

        // Returns the composite key used later by amend-style APIs.
        Q_INVOKABLE void add(const QString& name, const QString& iban, int currency_index);
        Q_INVOKABLE bool remove(int row);
        Q_INVOKABLE void toggle_favorite(int row);
        Q_INVOKABLE void mark_used_by_iban(const QString& iban);
        Q_INVOKABLE QVariantMap get(int row) const;

        QVector<beneficiary> beneficiaries() const { return items_; }

    signals:
        void changed();

    private:
        void sort_favorites_first();

        QVector<beneficiary> items_;
    };

} // namespace kmx
