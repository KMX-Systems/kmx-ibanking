/// @file src/services/account_service.cpp
/// @brief Ledger merge, normalization and local transaction posting.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "account_service.h"
#include "services/normalization.h"
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QStandardPaths>
#include <QTextStream>

namespace kmx
{
    namespace detail
    {

        // Remote DTO -> unified domain transaction, applying the normalization
        // pipeline for banks that deliver raw merchant strings.
        transaction to_domain(const connector::remote_transaction& r)
        {
            transaction t;
            t.direction = r.direction;
            t.counterparty = clean_merchant_name(r.counterparty_raw);
            if (r.has_counterparty_iban)
                t.counterparty_iban = r.counterparty_iban;
            t.amount_minor = r.amount_minor;
            t.currency = r.amount_currency;
            t.posted_at = r.posted_at;
            t.status = r.status;

            if (r.category_source == category_origin::native)
            {
                t.category_source = category_origin::native;
                t.category = r.category;
            }
            else
            {
                // Bank gave no usable category: aggregator infers it and stays honest
                // about provenance (surfaced later as an "auto-categorized" chip).
                t.category_source = category_origin::inferred;
                t.category = infer_category_from_merchant(r.counterparty_raw);
            }
            return t;
        }

    } // namespace detail
    account_service::account_service(QObject* parent): QObject(parent)
    {
    }

    QString account_service::key_for(bank_id bank, const QString& external_id) const
    {
        return QStringLiteral("%1:%2").arg(static_cast<int>(bank)).arg(external_id);
    }

    void account_service::upsert_accounts(bank_id bank, const QVector<connector::remote_account>& accounts)
    {
        bool changed = false;

        for (const auto& r: accounts)
        {
            const QString key = key_for(bank, r.external_id);
            const auto it = account_index_by_key_.constFind(key);

            if (it != account_index_by_key_.constEnd())
            {
                account& a = accounts_[it.value()];
                a.name = r.name;
                a.balance_minor = r.balance_minor;
                a.pending_hold_minor = r.pending_hold_minor;
                changed = true;
                continue;
            }

            account a;
            a.id = static_cast<qint64>(accounts_.size()) + 1;
            a.bank = bank;
            a.name = r.name;
            a.kind = r.kind;
            a.currency = r.currency;
            a.iban = r.iban;
            a.balance_minor = r.balance_minor;
            a.pending_hold_minor = r.pending_hold_minor;
            account_index_by_key_.insert(key, static_cast<qint64>(accounts_.size()));
            accounts_.append(a);
            changed = true;
        }

        if (changed)
            emit accounts_changed();
    }

    const account* account_service::account_by_key(const QString& key) const
    {
        const auto it = account_index_by_key_.constFind(key);
        if (it == account_index_by_key_.constEnd())
            return nullptr;
        return &accounts_[it.value()];
    }

    int account_service::merge_transactions(bank_id bank, const QVector<connector::remote_transaction>& txns)
    {
        int inserted = 0;

        for (const auto& r: txns)
        {
            const QString txn_key = key_for(bank, r.external_id);
            if (txn_by_external_key_.contains(txn_key))
                continue;

            transaction t = detail::to_domain(r);
            const auto acc_it = account_index_by_key_.constFind(key_for(bank, r.account_external_id));
            if (acc_it != account_index_by_key_.constEnd())
                t.account_id = accounts_[acc_it.value()].id;
            t.reference = txn_key; // external identity travels with the row

            txn_by_external_key_.insert(txn_key, t);
            ++inserted;
        }

        if (inserted > 0)
        {
            rebuild_ordered_view();
            emit transactions_merged(inserted, static_cast<int>(bank));
        }
        return inserted;
    }

    bool account_service::post_local_transaction(qint64 account_id, txn_direction direction, const QString& counterparty,
                                                 const QString& counterparty_iban, qint64 amount_minor, const QString& note,
                                                 QString* out_reference, txn_category category, const QString& fx_note)
    {
        account* target = nullptr;
        for (auto& a: accounts_)
            if (a.id == account_id)
            {
                target = &a;
                break;
            }
        if (!target)
            return false;

        const QString ref = QStringLiteral("PM-%1").arg(++local_seq_);
        const QString key = QStringLiteral("local:%1").arg(ref);

        transaction t;
        t.account_id = account_id;
        t.direction = direction;
        t.counterparty = counterparty;
        t.counterparty_iban = counterparty_iban;
        t.category = category;
        t.category_source = category_origin::native;
        t.fx_note = fx_note;
        t.amount_minor = amount_minor;
        t.currency = target->currency;
        t.posted_at = QDateTime::currentDateTime();
        t.status = txn_status::booked;
        t.reference = ref;
        t.note = note;

        if (direction == txn_direction::debit)
            target->balance_minor -= amount_minor;
        else
            target->balance_minor += amount_minor;

        txn_by_external_key_.insert(key, t);
        rebuild_ordered_view();

        emit accounts_changed();
        emit transactions_merged(1, static_cast<int>(target->bank));
        emit ledger_amended();
        if (out_reference)
            *out_reference = ref;
        return true;
    }

    void account_service::merge_transactions_for_test(const QVector<transaction>& rows)
    {
        bool changed = false;
        for (const auto& t: rows)
        {
            const QString key = QStringLiteral("test:%1").arg(t.reference);
            if (txn_by_external_key_.contains(key))
                continue;
            transaction copy = t;
            copy.reference = key;
            txn_by_external_key_.insert(key, copy);
            changed = true;
        }
        if (changed)
        {
            rebuild_ordered_view();
            emit ledger_amended();
            emit transactions_merged(rows.size(), 0);
        }
    }

    bool account_service::amend_transaction(const QString& external_key, int new_category, const QString& note)
    {
        auto it = txn_by_external_key_.find(external_key);
        if (it == txn_by_external_key_.end())
            return false;

        it->category = static_cast<txn_category>(new_category);
        it->category_source = category_origin::manual;
        if (!note.isNull())
            it->note = note;

        rebuild_ordered_view();
        emit ledger_amended();
        return true;
    }

    QString account_service::export_csv(const QVector<transaction>& rows) const
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString path =
            dir + QStringLiteral("/kmx-ledger-%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return {};

        // BOM keeps Excel/LibreOffice on UTF-8; RFC-4180 quoting throughout.
        file.write("\xef\xbb\xbf");

        QTextStream out(&file);
        out.setLocale(QLocale::c()); // dot decimals regardless of system locale
        out << "date;direction;counterparty;counterparty_iban;category;status;"
               "amount;amount_minor;currency;fx_note;note;reference\n";

        for (const auto& t: rows)
        {
            const auto esc = [](QString v)
            {
                v.replace('"', QStringLiteral("\"\""));
                return QStringLiteral("\"%1\"").arg(v);
            };
            out << esc(t.posted_at.toString(Qt::ISODate)) << ';'
                << esc(t.direction == txn_direction::credit ? QStringLiteral("credit") : QStringLiteral("debit")) << ';'
                << esc(t.counterparty) << ';' << esc(t.counterparty_iban) << ';' << esc(QLatin1String(category_label(t.category))) << ';'
                << esc(t.status == txn_status::pending ? QStringLiteral("pending") : QStringLiteral("booked")) << ';'
                << t.signed_amount_minor() << ';' << QString::number(t.signed_amount_minor() / 100.0, 'f', 2) << ';'
                << esc(QLatin1String(to_code(t.currency))) << ';' << esc(t.fx_note) << ';' << esc(t.note) << ';' << esc(t.reference)
                << '\n';
        }
        return path;
    }

    void account_service::rebuild_ordered_view()
    {
        transactions_ = txn_by_external_key_.values();
        std::sort(transactions_.begin(), transactions_.end(),
                  [](const transaction& a, const transaction& b)
                  {
                      if (a.posted_at != b.posted_at)
                          return a.posted_at > b.posted_at;
                      return a.reference < b.reference;
                  });
    }

} // namespace kmx
