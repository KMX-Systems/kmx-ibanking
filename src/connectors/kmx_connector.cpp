/// @file src/connectors/kmx_connector.cpp
/// @brief KMX Bank connector implementation over the seeded world.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "kmx_connector.h"

namespace kmx
{

    kmx_connector::kmx_connector(const seed_world& world, clock_source& clock): clock_(clock), world_(world)
    {
    }

    bank_capabilities kmx_connector::capabilities() const
    {
        return {/*pending_txns*/ true, /*auto_categories*/ true,
                /*virtual_cards*/ true, /*scheduled_payments*/ true};
    }

    fx_desk kmx_connector::desk() const
    {
        // Plan §2: all pairs at 15 bps spread, no fee.
        fx_desk desk(bank_id::kmx_bank);
        const auto add_pair = [&](currency_code from, currency_code to)
        {
            desk.add_rule({from, to, /*spread_bps*/ 15, /*fee_bps*/ 0, /*fee_fixed_minor*/ 0,
                           /*min_ticket_minor*/ 1'000, /*max_ticket_minor*/ 0});
        };
        add_pair(currency_code::ron, currency_code::eur);
        add_pair(currency_code::ron, currency_code::usd);
        add_pair(currency_code::eur, currency_code::ron);
        add_pair(currency_code::eur, currency_code::usd);
        add_pair(currency_code::usd, currency_code::ron);
        add_pair(currency_code::usd, currency_code::eur);
        return desk;
    }

    std::expected<connector::remote_session, connector::sync_error> kmx_connector::authenticate(
        const connector::mock_credentials& credentials)
    {
        if (credentials.password.isEmpty())
            return std::unexpected(
                connector::make_error(connector::sync_error_code::invalid_credentials, QStringLiteral("Password must not be empty")));

        connector::remote_session s;
        s.bank = bank();
        s.token = QStringLiteral("kmx-session");
        s.issued_at = clock_.now();
        s.validity_minutes = std::chrono::minutes(24 * 60);
        return s;
    }

    std::expected<QVector<connector::remote_account>, connector::sync_error> kmx_connector::fetch_accounts(
        const connector::remote_session& session)
    {
        if (!session.token.startsWith(QStringLiteral("kmx-")))
            return std::unexpected(
                connector::make_error(connector::sync_error_code::session_expired, QStringLiteral("Unknown session token")));

        QVector<connector::remote_account> out;
        for (const auto& a: world_.accounts)
        {
            if (a.bank != bank_id::kmx_bank)
                continue;
            connector::remote_account r;
            r.external_id = external_id_for(a.id);
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

    std::expected<QVector<connector::remote_transaction>, connector::sync_error> kmx_connector::fetch_transactions(
        const connector::remote_session& session, const QString& account_external_id, std::chrono::seconds since)
    {
        if (!session.token.startsWith(QStringLiteral("kmx-")))
            return std::unexpected(
                connector::make_error(connector::sync_error_code::session_expired, QStringLiteral("Unknown session token")));

        bool known_account = false;
        for (const auto& a: world_.accounts)
            if (a.bank == bank_id::kmx_bank && external_id_for(a.id) == account_external_id)
                known_account = true;
        if (!known_account)
            return std::unexpected(connector::make_error(connector::sync_error_code::unavailable,
                                                         QStringLiteral("Unknown account: %1").arg(account_external_id)));

        const QDateTime cutoff = QDateTime::currentDateTime().addSecs(static_cast<qint64>(since.count()) * -1);

        QVector<connector::remote_transaction> out;
        for (const auto& t: world_.transactions)
        {
            if (t.account_id < 1 || t.account_id > 3) // only KMX-owned accounts
                continue;
            if (external_id_for(t.account_id) != account_external_id || t.posted_at < cutoff)
                continue;

            connector::remote_transaction r;
            r.external_id = QStringLiteral("KMX-TXN-%1").arg(t.id);
            r.account_external_id = account_external_id;
            r.direction = t.direction;
            r.counterparty_raw = t.counterparty;
            r.has_counterparty_iban = !t.counterparty_iban.isEmpty();
            r.counterparty_iban = t.counterparty_iban;
            r.amount_minor = t.amount_minor;
            r.amount_currency = t.currency;
            r.posted_at = t.posted_at;
            r.status = t.status;
            r.category_source = category_origin::native; // KMX tags natively
            r.category = t.category;
            out.append(r);
        }
        return out;
    }

} // namespace kmx
