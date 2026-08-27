/// @file src/viewmodels/transaction_filter_proxy.cpp
/// @brief Search folding, filter composition and CSV export.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "transaction_filter_proxy.h"
#include <QHash>
#include <array>

namespace kmx
{
    namespace detail
    {

        // Folds Romanian (and general Latin-1) diacritics to plain ASCII so search is
        // keyboard-layout friendly: ăâîșț -> aais t etc.
        QString fold_diacritics(QString text)
        {
            struct fold
            {
                const char16_t from;
                const char to;
            };
            static constexpr std::array<fold, 12> folds {{
                {u'ă', 'a'},
                {u'â', 'a'},
                {u'î', 'i'},
                {u'ș', 's'},
                {u'ț', 't'},
                {u'ş', 's'},
                {u'Ă', 'a'},
                {u'Â', 'a'},
                {u'Î', 'i'},
                {u'Ș', 's'},
                {u'Ț', 't'},
                {u'Ş', 's'},
            }};
            for (const auto& f: folds)
                text.replace(QChar(f.from), QChar(f.to));
            return text.toLower();
        }

        bool contains_id(const QVariantList& list, int id)
        {
            if (list.isEmpty())
                return true;
            for (const auto& v: list)
                if (v.toInt() == id)
                    return true;
            return false;
        }

    } // namespace detail
    transaction_filter_proxy::transaction_filter_proxy(QObject* parent): QSortFilterProxyModel(parent)
    {
    }

    void transaction_filter_proxy::set_search_text(const QString& text)
    {
        if (search_text_ == text)
            return;
        beginFilterChange();
        search_text_ = text;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_categories(const QVariantList& ids)
    {
        if (categories_ == ids)
            return;
        beginFilterChange();
        categories_ = ids;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_bank_ids(const QVariantList& ids)
    {
        if (bank_ids_ == ids)
            return;
        beginFilterChange();
        bank_ids_ = ids;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_direction(int direction)
    {
        if (direction_ == direction)
            return;
        beginFilterChange();
        direction_ = direction;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_from_date(const QDate& date)
    {
        if (from_ == date)
            return;
        beginFilterChange();
        from_ = date;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_to_date(const QDate& date)
    {
        if (to_ == date)
            return;
        beginFilterChange();
        to_ = date;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::set_account_id_filter(qint64 account_id)
    {
        if (account_id_ == account_id)
            return;
        beginFilterChange();
        account_id_ = account_id;
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::clear_dates()
    {
        if (!from_.isValid() && !to_.isValid())
            return;
        beginFilterChange();
        from_ = QDate();
        to_ = QDate();
        endFilterChange();
        emit filter_changed();
    }

    void transaction_filter_proxy::clear()
    {
        beginFilterChange();
        search_text_.clear();
        categories_.clear();
        bank_ids_.clear();
        direction_ = -1;
        from_ = QDate();
        to_ = QDate();
        account_id_ = -1;
        endFilterChange();
        emit filter_changed();
    }

    QVector<transaction> transaction_filter_proxy::visible_transactions() const
    {
        QVector<transaction> out;
        const auto* src = qobject_cast<recent_transactions_model*>(sourceModel());
        if (!src)
            return out;

        QHash<QString, transaction> by_reference;
        by_reference.reserve(src->rowCount());
        for (const auto& t: src->transactions())
            by_reference.insert(t.reference, t);

        out.reserve(rowCount());
        for (int r = 0; r < rowCount(); ++r)
        {
            const QModelIndex idx = mapToSource(index(r, 0));
            const QVariant ref = src->data(idx, recent_transactions_model::reference_role);
            if (const auto it = by_reference.constFind(ref.toString()); it != by_reference.constEnd())
                out.append(it.value());
        }
        return out;
    }

    bool transaction_filter_proxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
    {
        const auto* src = qobject_cast<recent_transactions_model*>(sourceModel());
        if (!src)
            return false;

        const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
        const auto value = [&](int role) { return src->data(idx, role); };

        // account
        if (account_id_ >= 0)
        {
            // Resolve through the owning account id role stored on the row.
            const QVariant txn_id = value(recent_transactions_model::transaction_id_role);
            bool matches = false;
            for (const auto& t: src->transactions())
            {
                if (t.id != txn_id.toLongLong())
                    continue;
                matches = (t.account_id == account_id_);
                break;
            }
            if (!matches)
                return false;
        }

        // Direction
        if (direction_ >= 0 && value(recent_transactions_model::direction_role).toInt() != direction_)
            return false;

        // Category
        if (!detail::contains_id(categories_, value(recent_transactions_model::category_role).toInt()))
            return false;

        // Bank
        if (!detail::contains_id(bank_ids_, value(recent_transactions_model::bank_id_role).toInt()))
            return false;

        // Date range
        const QDateTime posted = value(recent_transactions_model::posted_at_role).toDateTime();
        if (from_.isValid() && posted.date() < from_)
            return false;
        if (to_.isValid() && posted.date() > to_)
            return false;

        // Search across counterparty + note + iban
        if (!search_text_.trimmed().isEmpty())
        {
            const QString needle = detail::fold_diacritics(search_text_);
            const QString haystack = detail::fold_diacritics(value(recent_transactions_model::counterparty_role).toString() + u' ' +
                                                             value(recent_transactions_model::note_role).toString() + u' ' +
                                                             value(recent_transactions_model::counterparty_iban_role).toString());
            if (!haystack.contains(needle))
                return false;
        }

        return true;
    }

} // namespace kmx
