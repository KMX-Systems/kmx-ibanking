/// @file src/viewmodels/transaction_filter_proxy.h
/// @brief Composable filters over the full ledger for the Transactions page.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDate>
    #include <QSortFilterProxyModel>
    #include <QVariantList>
#endif
#include "recent_transactions_model.h"
namespace kmx
{

    /// @brief Filter layer over the full ledger (plan §Phase 5).
    /// @details Every dimension is optional; set filters from QML, results re-evaluate immediately. Search folds Romanian diacritics so
    /// "buca" matches "Bucătăria".
    class transaction_filter_proxy: public QSortFilterProxyModel
    {
        Q_OBJECT
        Q_PROPERTY(QString search_text READ search_text WRITE set_search_text NOTIFY filter_changed)
        Q_PROPERTY(QVariantList categories READ categories WRITE set_categories NOTIFY filter_changed)
        Q_PROPERTY(QVariantList bank_ids READ bank_ids WRITE set_bank_ids NOTIFY filter_changed)
        Q_PROPERTY(int direction READ direction WRITE set_direction NOTIFY filter_changed) // -1 all
        Q_PROPERTY(QDate from_date READ from_date WRITE set_from_date NOTIFY filter_changed)
        Q_PROPERTY(QDate to_date READ to_date WRITE set_to_date NOTIFY filter_changed)
        Q_PROPERTY(qint64 account_id_filter READ account_id_filter WRITE set_account_id_filter NOTIFY filter_changed)

    public:
        explicit transaction_filter_proxy(QObject* parent = nullptr);

        QString search_text() const { return search_text_; }
        QVariantList categories() const { return categories_; }
        QVariantList bank_ids() const { return bank_ids_; }
        int direction() const { return direction_; }
        QDate from_date() const { return from_; }
        QDate to_date() const { return to_; }
        qint64 account_id_filter() const { return account_id_; }

        void set_search_text(const QString& text);
        void set_categories(const QVariantList& ids);
        void set_bank_ids(const QVariantList& ids);
        void set_direction(int direction);
        void set_from_date(const QDate& date);
        void set_to_date(const QDate& date);
        void set_account_id_filter(qint64 account_id);

        Q_INVOKABLE void clear();
        Q_INVOKABLE void clear_dates();

        // Rows currently visible, in export order (newest first).
        Q_INVOKABLE QVector<transaction> visible_transactions() const;

    signals:
        void filter_changed();

    protected:
        bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

    private:
        QString search_text_;
        QVariantList categories_; // ints; empty = all
        QVariantList bank_ids_;   // ints; empty = all
        int direction_ {-1};
        QDate from_;
        QDate to_;
        qint64 account_id_ {-1};
    };

} // namespace kmx
