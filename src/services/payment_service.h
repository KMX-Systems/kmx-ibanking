/// @file src/services/payment_service.h
/// @brief Transfer validation, execution and scheduled payments.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QObject>
    #include <QVector>
    #include <chrono>
    #include <expected>
    #include <functional>
#endif
#include "payment_types.h"
#include "services/clock_source.h"
namespace kmx
{

    class account_service;
    class card_service;
    class exchange_advisor_service;

    /// @brief Transfer execution + scheduled payments (plan §Phase 6).
    /// @details validate() runs the full error catalog synchronously; execute() applies latency theater then mutates the ledger (internal
    /// transfers post both legs) and emits transfer_completed/transfer_failed. Scheduled items recur monthly, gated by bank capabilities,
    /// with a run_due_now() demo trigger.
    class payment_service: public QObject
    {
        Q_OBJECT
    public:
        explicit payment_service(account_service& accounts, clock_source& clock, QObject* parent = nullptr);

        // Capability lookup injected to avoid a hard connection_service dependency.
        void set_capability_resolver(std::function<bool(int bank_id)> resolver) { capabilities_ = std::move(resolver); }

        // Demo-tunable latency (tests use zero).
        void set_latency(std::chrono::milliseconds delay) { latency_ = delay; }

        // P7: routed cross-currency execution.
        void set_advisor(exchange_advisor_service* advisor) { advisor_ = advisor; }
        void set_card_service(card_service* cards) { cards_ = cards; }
        Q_INVOKABLE bool execute_exchange(const QVariantMap& route);

        static constexpr qint64 min_transfer_minor = 100;       // 1.00
        static constexpr qint64 max_transfer_minor = 50'000'00; // 50 000.00 per transfer

        std::expected<validated_transfer, payment_error> validate(const transfer_request& request) const;

        void execute(const validated_transfer& validated); // async under latency

        // QML entry point: validates then executes; failures surface via
        // transfer_failed synchronously when latency is zero.
        Q_INVOKABLE bool submit(const QVariantMap& request);

        Q_INVOKABLE QVariantMap schedule_from_map(const QVariantMap& request, int day_of_month);
        Q_INVOKABLE QVariantList scheduled_list() const;

        // ---- scheduled payments ---------------------------------------------
        const QVector<scheduled_payment>& scheduled() const { return scheduled_; }

        std::expected<qint64, payment_error> schedule(const transfer_request& request, int day_of_month);
        Q_INVOKABLE bool cancel_scheduled(qint64 id);
        // Executes standing orders whose date has come; force=true runs them
        // regardless of schedule (the demo-menu trigger).
        Q_INVOKABLE int run_due_now(bool force = false);

        void tick(); // 1 Hz from the session timer

    signals:
        void scheduled_changed();
        void transfer_completed(const QVariantMap& receipt);
        void transfer_failed(const QString& code, const QString& message);
        void schedule_completed(const QVariantMap& receipt);

    private:
        const account* find_account(qint64 id) const;
        qint64 resolve_internal_destination(const QString& iban) const;
        void finish_execution(const validated_transfer& v);
        scheduled_payment* find_scheduled(qint64 id);
        qint64 find_largest_at(int bank_id, currency_code ccy, qint64 exclude_account_id) const;

        account_service& accounts_;
        exchange_advisor_service* advisor_ {nullptr};
        card_service* cards_ {nullptr};
        clock_source& clock_;
        std::function<bool(int)> capabilities_ {[](int) { return false; }};
        std::chrono::milliseconds latency_ {900};
        QVector<scheduled_payment> scheduled_;
        qint64 next_schedule_id_ {1};
    };

} // namespace kmx
