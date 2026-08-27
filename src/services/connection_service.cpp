/// @file src/services/connection_service.cpp
/// @brief Connect, refresh, disconnect and the auto-sync scheduler.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "connection_service.h"
#include "domain/fx_desk.h"
#include "services/account_service.h"
#include "services/notification_service.h"
#include <QTimer>

namespace kmx
{

    connection_service::connection_service(clock_source& clock, QObject* parent): QObject(parent), clock_(clock)
    {
        // 1 Hz scheduler: staggered auto-syncs fire from here (plan §3).
        auto* scheduler = new QTimer(this);
        scheduler->setInterval(1000);
        connect(scheduler, &QTimer::timeout, this, &connection_service::on_scheduler_tick);
        scheduler->start();
    }

    void connection_service::register_connector(std::unique_ptr<bank_connector> connector)
    {
        Q_ASSERT(connector);
        const int slot = static_cast<int>(connector->bank());
        Q_ASSERT(slot >= 0 && slot < bank_count);
        connection conn;
        conn.connector = std::move(connector);
        connections_[static_cast<size_t>(slot)] = std::move(conn);
    }

    void connection_service::set_sync_delay(bank_id bank, std::chrono::milliseconds delay)
    {
        if (auto* c = find(bank))
            c->sync_delay = delay;
    }

    void connection_service::set_auto_sync_interval(bank_id bank, qint64 interval_ms)
    {
        if (auto* c = find(bank))
            c->auto_sync_interval_ms = interval_ms;
    }

    void connection_service::set_state(connection& c, bank_id bank, link_state next)
    {
        if (c.state == next)
            return;
        c.state = next;
        emit state_changed(static_cast<int>(bank), static_cast<int>(next));
    }

    void connection_service::notify(const QString& level, const QString& title, const QString& body, const QString& deep_link_key)
    {
        if (notifications_)
            notifications_->post(level, title, body, deep_link_key);
    }

    std::expected<void, connector::sync_error> connection_service::connect_bank(bank_id bank, const connector::mock_credentials& creds)
    {
        connection* c = find(bank);
        if (!c)
            return std::unexpected(
                connector::make_error(connector::sync_error_code::unavailable, QStringLiteral("No connector registered for this bank")));

        set_state(*c, bank, link_state::Authenticating);

        auto session = c->connector->authenticate(creds);
        if (!session)
        {
            set_state(*c, bank, link_state::Disconnected);
            c->session.reset();
            return std::unexpected(session.error());
        }

        c->session = *session;
        set_state(*c, bank, link_state::Connected);

        // Initial pull goes through the same async path as any refresh so the
        // onboarding UI gets its skeletons; with zero delay it is synchronous.
        // Linking succeeded even if this first pull fails: the outcome shows up
        // as sync_error/NeedsReauth state and recovers through refresh().
        refresh(bank);
        return {};
    }

    bool connection_service::refresh(bank_id bank)
    {
        connection* c = find(bank);
        if (!c)
            return false;

        if (c->syncing)
            return true; // already in flight

        if (c->state == link_state::NeedsReauth || c->state == link_state::Disconnected || c->state == link_state::Authenticating)
            return false;

        if (!c->session || c->session->is_expired(clock_.now()))
        {
            // Sessions never silently re-authenticate: open banking links demand
            // explicit user action — surface it loudly once.
            const bool was_not_reauth = c->state != link_state::NeedsReauth;
            c->session.reset();
            set_state(*c, bank, link_state::NeedsReauth);
            if (was_not_reauth)
            {
                notify(QStringLiteral("warning"), QString::fromLatin1(bank_name(bank)), QObject::tr("Session expired — reconnect required"),
                       QStringLiteral("connections"));
            }
            return false;
        }

        c->syncing = true;
        emit sync_started(static_cast<int>(bank));

        if (c->sync_delay.count() <= 0)
            run_sync(bank);
        else
            QTimer::singleShot(c->sync_delay, this, [this, bank]() { run_sync(bank); });
        return true;
    }

    void connection_service::run_sync(bank_id bank)
    {
        connection* c = find(bank);
        if (!c || !c->syncing)
            return;
        c->syncing = false;

        // A disconnect() (or expired-session reset) may have landed between the
        // refresh() scheduling and this deferred execution.
        if (!c->session)
        {
            set_state(*c, bank, link_state::NeedsReauth);
            emit sync_finished(static_cast<int>(bank), false, 0);
            return;
        }

        auto result = perform_sync(*c);
        if (result.has_value())
        {
            set_state(*c, bank, link_state::Connected);
            c->last_error.clear(); // rate-limit hints must not outlive recovery
            emit sync_finished(static_cast<int>(bank), true, result.value());
        }
        else
        {
            handle_sync_failure(*c, result.error());
            emit sync_finished(static_cast<int>(bank), false, 0);
        }
    }

    std::expected<int, connector::sync_error> connection_service::perform_sync(connection& c)
    {
        const connector::remote_session session = *c.session;

        auto accounts = c.connector->fetch_accounts(session);
        if (!accounts)
            return std::unexpected(accounts.error());

        if (accounts_)
            accounts_->upsert_accounts(c.connector->bank(), *accounts);

        int new_total = 0;
        for (const auto& acc: *accounts)
        {
            auto txns = c.connector->fetch_transactions(session, acc.external_id, std::chrono::hours(24 * 90));
            if (!txns)
                return std::unexpected(txns.error());
            if (accounts_)
                new_total += accounts_->merge_transactions(c.connector->bank(), *txns);
        }

        c.last_sync = clock_.now();
        return new_total;
    }

    void connection_service::handle_sync_failure(connection& c, const connector::sync_error& error)
    {
        const bank_id bank = c.connector->bank();

        switch (error.code)
        {
            case connector::sync_error_code::session_expired:
                c.session.reset();
                set_state(c, bank, link_state::NeedsReauth);
                notify(QStringLiteral("warning"), QString::fromLatin1(bank_name(bank)), QObject::tr("Session expired — reconnect required"),
                       QStringLiteral("connections"));
                break;
            case connector::sync_error_code::rate_limited:
                // Stays Connected; the UI greys refresh with the retry hint.
                c.last_error = error.detail.isEmpty() ? QStringLiteral("Rate limited") : error.detail;
                break;
            default:
                set_state(c, bank, link_state::SyncFailed);
                c.last_error = error.detail;
                break;
        }
    }

    void connection_service::on_scheduler_tick()
    {
        const QDateTime now = clock_.now();

        for (size_t i = 0; i < connections_.size(); ++i)
        {
            connection& c = connections_[i];
            if (c.state != link_state::Connected || c.syncing || c.auto_sync_interval_ms <= 0)
                continue;
            const qint64 since_ms = c.last_sync.isValid() ? c.last_sync.msecsTo(now) : c.auto_sync_interval_ms; // never synced: due now
            if (since_ms >= c.auto_sync_interval_ms)
                refresh(static_cast<bank_id>(static_cast<int>(i)));
        }
    }

    fx_desk connection_service::fx_desk_for(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c && c->connector ? c->connector->desk() : fx_desk();
    }

    bank_capabilities connection_service::connector_capabilities(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c && c->connector ? c->connector->capabilities() : bank_capabilities {};
    }

    void connection_service::expire_session_for_demo(int bank_id)
    {
        if (auto* c = find(static_cast<kmx::bank_id>(bank_id)))
            c->connector->expire_session_for_demo();
    }

    void connection_service::disconnect(bank_id bank)
    {
        connection* c = find(bank);
        if (!c)
            return;
        c->session.reset();
        c->last_sync = QDateTime();
        c->last_error.clear();
        set_state(*c, bank, link_state::Disconnected);
    }

    int connection_service::state(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c ? static_cast<int>(c->state) : static_cast<int>(link_state::Disconnected);
    }

    QDateTime connection_service::last_sync_at(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c ? c->last_sync : QDateTime();
    }

    QString connection_service::last_error_text(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c ? c->last_error : QString();
    }

    bool connection_service::is_syncing(int bank_id) const
    {
        const connection* c = find(static_cast<kmx::bank_id>(bank_id));
        return c ? c->syncing : false;
    }

} // namespace kmx
