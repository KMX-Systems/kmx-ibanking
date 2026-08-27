/// @file src/services/connection_service.h
/// @brief Per-bank link lifecycle, sync scheduling and rate limits.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QObject>
    #include <array>
    #include <chrono>
    #include <expected>
    #include <memory>
#endif
#include "connectors/bank_connector.h"
#include "domain/bank.h"
#include "services/clock_source.h"
namespace kmx
{

    class account_service;
    class notification_service;

    /// @brief Owns the per-bank link lifecycle (plan §3):
    /// @details Disconnected -> Authenticating -> Connected <-> NeedsReauth / sync_error. Sync runs through latency theater (per-bank
    /// delay, zero in tests); a 1 Hz internal scheduler fires staggered auto-syncs while links stay Connected. Fresh data lands in
    /// account_service; notable events reach notification_service.
    class connection_service: public QObject
    {
        Q_OBJECT
    public:
        /// @brief NOTE (style exception):
        /// @details the enumerators below stay PascalCase even though the guide asks for lowercase constants. Qt only exposes Q_ENUM keys
        /// to QML when they begin with an uppercase letter -- QML reads `ConnectionService.Connected`, and a lowercase key silently
        /// evaluates to `undefined` (verified, not assumed).
        enum class link_state : quint8
        {
            Disconnected = 0,
            Authenticating,
            Connected,
            NeedsReauth,
            SyncFailed
        };
        Q_ENUM(link_state)

        explicit connection_service(clock_source& clock, QObject* parent = nullptr);

        void register_connector(std::unique_ptr<bank_connector> connector);

        // Wiring for aggregation + alerts.
        void set_account_service(account_service* accounts) { accounts_ = accounts; }
        void set_notification_service(notification_service* notifications) { notifications_ = notifications; }

        // Per-bank tuning (main.cpp sets demo values; tests keep zeros).
        void set_sync_delay(bank_id bank, std::chrono::milliseconds delay);
        void set_auto_sync_interval(bank_id bank, qint64 interval_ms);

        std::expected<void, connector::sync_error> connect_bank(bank_id bank, const connector::mock_credentials& creds);
        bool refresh(bank_id bank); // async; false only on immediate rejection
        void disconnect(bank_id bank);

        Q_INVOKABLE int state(int bank_id) const;
        Q_INVOKABLE QDateTime last_sync_at(int bank_id) const;
        Q_INVOKABLE QString last_error_text(int bank_id) const;
        Q_INVOKABLE bool is_syncing(int bank_id) const;
        Q_INVOKABLE void expire_session_for_demo(int bank_id);

        // QML-facing wrappers (bank_id/struct signatures aren't QML-callable).
        Q_INVOKABLE bool link_bank(int bank_id, const QString& username, const QString& password, const QString& otp)
        {
            return connect_bank(static_cast<kmx::bank_id>(bank_id), {username, password, otp}).has_value();
        }
        Q_INVOKABLE bool refresh_bank(int bank_id) { return refresh(static_cast<kmx::bank_id>(bank_id)); }
        Q_INVOKABLE void disconnect_bank(int bank_id) { disconnect(static_cast<kmx::bank_id>(bank_id)); }
        bank_capabilities connector_capabilities(int bank_id) const;
        fx_desk fx_desk_for(int bank_id) const;
        Q_INVOKABLE bool supports_virtual_cards(int bank_id) const { return connector_capabilities(bank_id).virtual_cards; }
        bool is_connected(bank_id bank) const { return state(static_cast<int>(bank)) == static_cast<int>(link_state::Connected); }

    signals:
        void state_changed(int bank_id, int new_state);
        void sync_started(int bank_id);
        void sync_finished(int bank_id, bool ok, int new_transactions);

    private slots:
        void on_scheduler_tick();

    private:
        struct connection
        {
            std::unique_ptr<bank_connector> connector;
            link_state state {link_state::Disconnected};
            std::optional<connector::remote_session> session;
            QDateTime last_sync;
            QString last_error;
            std::chrono::milliseconds sync_delay {0};
            qint64 auto_sync_interval_ms {0};
            bool syncing {false};
        };

        const connection* find(bank_id bank) const
        {
            const int i = static_cast<int>(bank);
            return (i >= 0 && i < bank_count) ? &connections_[static_cast<size_t>(i)] : nullptr;
        }
        connection* find(bank_id bank) { return const_cast<connection*>(static_cast<const connection_service*>(this)->find(bank)); }

        void set_state(connection& c, bank_id bank, link_state next);
        void run_sync(bank_id bank);
        std::expected<int, connector::sync_error> perform_sync(connection& c);
        void handle_sync_failure(connection& c, const connector::sync_error& error);
        void notify(const QString& level, const QString& title, const QString& body, const QString& deep_link_key);

        clock_source& clock_;
        account_service* accounts_ {nullptr};
        notification_service* notifications_ {nullptr};
        std::array<connection, static_cast<size_t>(bank_count)> connections_;
    };

} // namespace kmx
