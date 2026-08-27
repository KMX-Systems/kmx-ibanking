/// @file src/connectors/kmx_connector.h
/// @brief KMX Bank persona: instant sync, native categories, long sessions.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QVector>
#endif
#include "bank_connector.h"
#include "domain/seed_world.h"
namespace kmx
{

    /// @brief Reference implementation of bank_connector (plan §3):
    /// @details instant sync, long-lived sessions, native categories. Serves the KMX slice of the seeded world. Later banks diverge from
    /// this baseline.
    class kmx_connector final: public bank_connector
    {
    public:
        explicit kmx_connector(const seed_world& world, clock_source& clock);

        bank_id bank() const override { return bank_id::kmx_bank; }
        bank_capabilities capabilities() const override;
        fx_desk desk() const override;

        std::expected<connector::remote_session, connector::sync_error> authenticate(
            const connector::mock_credentials& credentials) override;

        std::expected<QVector<connector::remote_account>, connector::sync_error> fetch_accounts(
            const connector::remote_session& session) override;

        std::expected<QVector<connector::remote_transaction>, connector::sync_error> fetch_transactions(
            const connector::remote_session& session, const QString& account_external_id, std::chrono::seconds since) override;

    private:
        QString external_id_for(account_id_t id) const { return QStringLiteral("KMX-ACC-%1").arg(id); }

        clock_source& clock_;
        const seed_world& world_;
    };

} // namespace kmx
