/// @file src/connectors/seed_bank_connector.cpp
/// @brief Seeded-world fetch, session and rate-limit behaviour shared by banks.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "seed_bank_connector.h"
#include "services/normalization.h"
#include <QDateTime>

namespace kmx
{

    seed_bank_connector::seed_bank_connector(bank_id bank, const seed_world& world, options options, clock_source& clock):
        bank_(bank),
        clock_(clock),
        world_(world),
        options_(options)
    {
    }

    bank_capabilities seed_bank_connector::capabilities() const
    {
        bank_capabilities caps;
        caps.pending_txns = !options_.booked_only;
        switch (options_.categories)
        {
            case options::category_quality::native:
                caps.auto_categories = true;
                break;
            case options::category_quality::partial:
                caps.auto_categories = false;
                break;
            case options::category_quality::inferred:
                caps.auto_categories = false;
                break;
        }
        // Only KMX offers virtual cards + scheduled payments in this demo.
        caps.virtual_cards = (bank_ == bank_id::kmx_bank);
        caps.scheduled_payments = (bank_ == bank_id::kmx_bank || bank_ == bank_id::banca_transilvania);
        return caps;
    }

    QString seed_bank_connector::external_account_id(const account& a) const
    {
        return QStringLiteral("BK%1-ACC-%2").arg(static_cast<int>(bank_)).arg(a.id);
    }

    QString seed_bank_connector::external_txn_id(const transaction& t) const
    {
        return QStringLiteral("BK%1-TXN-%2").arg(static_cast<int>(bank_)).arg(t.id);
    }

    std::expected<connector::remote_session, connector::sync_error> seed_bank_connector::authenticate(
        const connector::mock_credentials& credentials)
    {
        if (credentials.password.isEmpty())
            return std::unexpected(
                connector::make_error(connector::sync_error_code::invalid_credentials, QStringLiteral("Password must not be empty")));

        session_revoked_ = false;

        connector::remote_session s;
        s.bank = bank_;
        s.token = QStringLiteral("bk%1-session").arg(static_cast<int>(bank_));
        s.issued_at = clock_.now();
        s.validity_minutes = options_.session_validity;
        return s;
    }

    std::expected<QVector<connector::remote_account>, connector::sync_error> seed_bank_connector::fetch_accounts(
        const connector::remote_session& session)
    {
        if (session.bank != bank_ || session.token.isEmpty() || session_revoked_)
            return std::unexpected(connector::make_error(connector::sync_error_code::session_expired, QStringLiteral("Session rejected")));

        QVector<connector::remote_account> out;
        for (const auto& a: world_.accounts)
        {
            if (a.bank != bank_)
                continue;
            connector::remote_account r;
            r.external_id = external_account_id(a);
            r.name = a.name;
            r.kind = a.kind;
            r.currency = a.currency;
            r.iban = a.iban;
            r.balance_minor = a.balance_minor;
            r.pending_hold_minor = a.pending_hold_minor;
            out.append(r);
        }
        return out;
    }

    std::expected<QVector<connector::remote_transaction>, connector::sync_error> seed_bank_connector::fetch_transactions(
        const connector::remote_session& session, const QString& account_external_id, std::chrono::seconds since)
    {
        if (auto guard = pre_fetch_guard())
            return std::unexpected(*guard);

        if (session.bank != bank_ || session.token.isEmpty() || session_revoked_)
            return std::unexpected(connector::make_error(connector::sync_error_code::session_expired, QStringLiteral("Session rejected")));

        bool known = false;
        for (const auto& a: world_.accounts)
            if (a.bank == bank_ && external_account_id(a) == account_external_id)
                known = true;
        if (!known)
            return std::unexpected(connector::make_error(connector::sync_error_code::unavailable,
                                                         QStringLiteral("Unknown account: %1").arg(account_external_id)));

        const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-static_cast<qint64>(since.count()));

        QVector<connector::remote_transaction> out;
        for (const auto& t: world_.transactions)
        {
            if (t.posted_at < cutoff)
                continue;
            const account* owner = world_.account_by_id(t.account_id);
            if (!owner || owner->bank != bank_)
                continue;
            if (external_account_id(*owner) != account_external_id)
                continue;

            connector::remote_transaction r;
            r.external_id = external_txn_id(t);
            r.account_external_id = account_external_id;
            r.direction = t.direction;
            r.amount_minor = t.amount_minor;
            r.amount_currency = t.currency;
            r.posted_at = t.posted_at;
            r.status = t.status;

            // ---- quirk matrix -------------------------------------------------
            if (options_.booked_only && t.status == txn_status::pending)
                continue; // bank simply doesn't report pending items yet

            if (options_.raw_merchant_strings)
            {
                r.counterparty_raw = t.counterparty.toUpper() + QStringLiteral(" RO 0123");
                r.category_source = category_origin::inferred; // aggregator must infer
            }
            else
            {
                r.counterparty_raw = t.counterparty;
                switch (options_.categories)
                {
                    case options::category_quality::native:
                        r.category_source = category_origin::native;
                        r.category = t.category;
                        break;
                    case options::category_quality::partial:
                        if (merchant_looks_known(t.counterparty))
                        {
                            r.category_source = category_origin::native;
                            r.category = t.category;
                        }
                        else
                        {
                            r.category_source = category_origin::inferred;
                        }
                        break;
                    case options::category_quality::inferred:
                        r.category_source = category_origin::inferred;
                        break;
                }
            }

            if (options_.sparse_metadata || t.counterparty_iban.isEmpty())
            {
                r.has_counterparty_iban = false;
            }
            else
            {
                r.has_counterparty_iban = true;
                r.counterparty_iban = t.counterparty_iban;
            }

            out.append(r);
        }
        return out;
    }

} // namespace kmx
