/// @file src/services/account_service.h
/// @brief Unified cross-bank ledger: upsert, dedupe and normalize.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QHash>
    #include <QObject>
    #include <QVector>
#endif
#include "connectors/connector_types.h"
#include "domain/account.h"
#include "domain/bank.h"
#include "domain/transaction.h"
namespace kmx
{

    /// @brief Unified ledger across linked banks (plan §3):
    /// @details connectors deliver DTOs, this store normalizes and merges them. Grouping by bank is presentation- only; downstream sees one
    /// flat universe. Identity model: every remote entity is keyed "bankId:external_id" so syncs can update in place; internal sequential
    /// ids are stable within a session.
    class account_service: public QObject
    {
        Q_OBJECT
    public:
        explicit account_service(QObject* parent = nullptr);

        // Updates balances/names of known accounts, appends unknown ones.
        void upsert_accounts(bank_id bank, const QVector<connector::remote_account>& accounts);

        // Dedupes by external identity; returns how many rows were new.
        int merge_transactions(bank_id bank, const QVector<connector::remote_transaction>& txns);

        QVector<account> accounts() const { return accounts_; }
        QVector<transaction> transactions() const { return transactions_; } // newest first
        int transaction_count() const { return static_cast<int>(transactions_.size()); }

        const account* account_by_key(const QString& key) const;

    signals:
        void accounts_changed();
        void transactions_merged(int new_count, int bank_id);
        void ledger_amended();

    public:
        // Manual recategorization from the detail sheet; provenance becomes Manual.
        bool amend_transaction(const QString& external_key, int new_category, const QString& note);

        // Local money movement (transfers): adjusts the stored balance and posts
        // the row into the ledger. Returns the generated reference via out_reference.
        bool post_local_transaction(qint64 account_id, txn_direction direction, const QString& counterparty,
                                    const QString& counterparty_iban, qint64 amount_minor, const QString& note,
                                    QString* out_reference = nullptr, txn_category category = txn_category::transfer,
                                    const QString& fx_note = {});

        // Composite external key for an account ("bankId:external-id"), used when
        // linking transfers back to remote identities later.

        // Test-only injection of fully specified rows (deterministic dates etc).
        void merge_transactions_for_test(const QVector<transaction>& rows);

        // RFC-4180 CSV (UTF-8 BOM) of the given rows; returns absolute path.
        QString export_csv(const QVector<transaction>& rows) const;

    private:
        QString key_for(bank_id bank, const QString& external_id) const;
        void rebuild_ordered_view();

        QVector<account> accounts_;
        QHash<QString, qint64> account_index_by_key_; // composite key -> index into accounts_
        QHash<QString, transaction> txn_by_external_key_;
        qint64 local_seq_ {0};
        QVector<transaction> transactions_;
    };

} // namespace kmx
