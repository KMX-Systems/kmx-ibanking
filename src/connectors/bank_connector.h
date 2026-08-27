/// @file src/connectors/bank_connector.h
/// @brief Interface every simulated bank connector implements.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QVector>
    #include <chrono>
    #include <expected>
#endif
#include "connector_types.h"
#include "domain/capabilities.h"
#include "domain/fx_desk.h"
namespace kmx
{

    /// @brief One implementation per simulated bank.
    /// @details Everything bank-specific — quirks, latency theater, session lifetimes, rate limits — lives behind this seam.
    class bank_connector
    {
    public:
        virtual ~bank_connector() = default;

        virtual bank_id bank() const = 0;

        // Demo-only hook (plan §3): forces the next guard check to treat the
        // stored session as revoked. Default: no-op.
        virtual void expire_session_for_demo() {}
        virtual bank_capabilities capabilities() const = 0;
        virtual fx_desk desk() const = 0;

        virtual std::expected<connector::remote_session, connector::sync_error> authenticate(
            const connector::mock_credentials& credentials) = 0;

        virtual std::expected<QVector<connector::remote_account>, connector::sync_error> fetch_accounts(
            const connector::remote_session& session) = 0;

        virtual std::expected<QVector<connector::remote_transaction>, connector::sync_error> fetch_transactions(
            const connector::remote_session& session, const QString& account_external_id, std::chrono::seconds since) = 0;
    };

} // namespace kmx
