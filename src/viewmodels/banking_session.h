/// @file src/viewmodels/banking_session.h
/// @brief The single QObject facade QML binds to as `bank`.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaEnum>
    #include <QObject>
    #include <QStringList>
#endif
#include "domain/bank.h"
#include "domain/budget.h"
#include "services/analytics_service.h"
#include "services/auth_service.h"
#include "services/budget_service.h"
#include "services/card_service.h"
#include "services/connection_service.h"
#include "services/exchange_advisor_service.h"
#include "services/fx_service.h"
#include "services/notification_service.h"
#include "services/payment_service.h"
#include "services/user_activity_monitor.h"
#include "viewmodels/account_model.h"
#include "viewmodels/beneficiary_model.h"
#include "viewmodels/card_model.h"
#include "viewmodels/recent_transactions_model.h"
#include "viewmodels/transaction_filter_proxy.h"
namespace kmx
{

    /// @brief Single QObject facade exposed to QML as `bank` (plan §1).
    /// @details Grows phase by phase; surfaces link status (P0) and the auth/session gate (P2). Headers are included (not forward-declared)
    /// because Q_PROPERTY pointers must be fully-defined for the metatype system.
    class banking_session: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString status_line READ status_line NOTIFY status_line_changed)
        Q_PROPERTY(auth_service* auth READ auth CONSTANT)
        Q_PROPERTY(connection_service* connections READ connections CONSTANT)
        Q_PROPERTY(user_activity_monitor* activity READ activity CONSTANT)
        Q_PROPERTY(notification_service* notifications READ notifications CONSTANT)

        // Display currency (0=RON 1=EUR 2=USD), persisted via QSettings.
        Q_PROPERTY(int display_currency READ display_currency WRITE set_display_currency NOTIFY display_currency_changed)
        // Cross-bank net worth in the display currency; refreshes on FX ticks.
        Q_PROPERTY(qint64 net_worth_minor READ net_worth_minor NOTIFY net_worth_changed)
    public:
        banking_session(auth_service& auth, connection_service& connections, user_activity_monitor& activity,
                        notification_service& notifications, account_service& accounts, fx_service& fx, payment_service& payments,
                        exchange_advisor_service& advisor, analytics_service& analytics, budget_service& budgets, clock_source& clock,
                        QObject* parent = nullptr);

        QString status_line() const { return status_line_; }

        auth_service* auth() const { return &auth_; }
        connection_service* connections() const { return &connections_; }
        user_activity_monitor* activity() const { return &activity_; }
        notification_service* notifications() const { return &notifications_; }

        Q_PROPERTY(account_list_model* account_model READ account_model CONSTANT)
        Q_PROPERTY(recent_transactions_model* recent_transactions READ recent_transactions CONSTANT)
        account_list_model* account_model() const { return account_model_; }
        recent_transactions_model* recent_transactions() const { return recent_txns_; }
        // Long feed bound to one account; the details page sets its filter.
        Q_PROPERTY(recent_transactions_model* statement_transactions READ statement_transactions CONSTANT)
        recent_transactions_model* statement_transactions() const { return statement_txns_; }
        // Full ledger + filter layer for the Transactions page.
        Q_PROPERTY(transaction_filter_proxy* ledger READ ledger CONSTANT)
        Q_PROPERTY(payment_service* payments READ payments CONSTANT)
        Q_PROPERTY(exchange_advisor_service* advisor READ advisor CONSTANT)
        Q_PROPERTY(beneficiary_model* beneficiaries READ beneficiaries CONSTANT)
        Q_PROPERTY(card_model* cards READ cards CONSTANT)
        Q_PROPERTY(analytics_service* analytics READ analytics CONSTANT)
        Q_PROPERTY(budget_service* budgets READ budgets CONSTANT)
        Q_PROPERTY(QVariantList bank_subtotals READ bank_subtotals NOTIFY accounts_changed)
        Q_PROPERTY(QVariantList budget_progress READ budget_progress NOTIFY accounts_changed)
        Q_INVOKABLE QVariantMap create_virtual_card(int account_id, const QString& label, qint64 daily_limit_minor);
        void set_cards(QVector<card> cards) { card_service_.set_cards(std::move(cards)); }
        class card_service* cards_service() { return &card_service_; }
        Q_INVOKABLE bool set_card_frozen(qint64 card_id, bool frozen) { return card_service_.set_frozen(card_id, frozen); }
        Q_INVOKABLE bool set_card_online(qint64 card_id, bool on) { return card_service_.set_online_payments(card_id, on); }
        Q_INVOKABLE bool set_card_contactless(qint64 card_id, bool on) { return card_service_.set_contactless(card_id, on); }
        Q_INVOKABLE QVariantMap set_card_limit(qint64 card_id, qint64 limit_minor)
        {
            return card_service_.set_daily_limit(card_id, limit_minor);
        }
        transaction_filter_proxy* ledger() const { return ledger_; }
        payment_service* payments() const { return &payments_; }
        exchange_advisor_service* advisor() const { return &advisor_; }
        beneficiary_model* beneficiaries() const { return beneficiary_model_; }
        card_model* cards() const { return card_model_; }
        analytics_service* analytics() const { return &analytics_; }
        budget_service* budgets() const { return &budgets_; }
        // Back-compat for the dashboard widget; delegates to budget_service.

        // Wizard helpers.
        Q_INVOKABLE bool is_valid_iban(const QString& iban) const;
        Q_INVOKABLE void set_budgets_and_beneficiaries(const QVector<budget>& budgets, QVector<beneficiary> beneficiaries);
        Q_INVOKABLE QString export_ledger_csv();
        Q_INVOKABLE int currency_for_iban(const QString& iban) const;
        // Threshold-gated insight cards for the dashboard (plan §6.3).
        Q_PROPERTY(QVariantList exchange_insights READ exchange_insights NOTIFY accounts_changed)
        Q_INVOKABLE QVariantList exchange_insights() const;
        Q_INVOKABLE void fx_shock() { fx_.shock(); }

        Q_INVOKABLE void set_language(const QString& locale) { emit language_change_requested(locale); }

        // Demo scenarios (plan §Phase 10).
        Q_INVOKABLE void simulate_incoming_salary();
        Q_INVOKABLE void simulate_fraud_alert();
        Q_INVOKABLE void simulate_new_device_login();
        Q_INVOKABLE double advisor_mid_rate(int from_ccy, int to_ccy) const
        {
            return fx_.mid(static_cast<currency_code>(from_ccy), static_cast<currency_code>(to_ccy));
        }
        Q_INVOKABLE QVariantList rate_history(int from_ccy, int to_ccy) const
        {
            QVariantList out;
            for (double v: fx_.history(static_cast<currency_code>(from_ccy), static_cast<currency_code>(to_ccy)))
                out.append(v);
            return out;
        }
        Q_INVOKABLE bool amend_transaction(const QString& reference_key, int new_category, const QString& note);

        Q_INVOKABLE QString account_name(int account_id) const;
        Q_INVOKABLE qint64 account_balance(int account_id) const;
        Q_INVOKABLE QString account_currency_code(int account_id) const;

        int display_currency() const;
        void set_display_currency(int currency_index);
        qint64 net_worth_minor() const;

        Q_INVOKABLE QVariantList bank_subtotals() const;
        Q_INVOKABLE QVariantList budget_progress() const;
        Q_INVOKABLE void accounts_changed_ping() { emit accounts_changed(); }
        Q_INVOKABLE QVariantList month_in_out(int account_id) const;
        Q_INVOKABLE QString account_iban(int account_id) const;
        Q_INVOKABLE void copy_to_clipboard(const QString& text);
        Q_INVOKABLE void tick_fx() { fx_.tick(); }

        // budget envelopes come straight from the seeded world.

        Q_INVOKABLE QStringList linked_banks() const;
        const account* find_account(int account_id) const;
        Q_INVOKABLE void force_bt_session_expiry();
        // Links the user's own bank right after the first successful login so
        // the app shows real data; other banks go through Connect-a-bank.
        Q_INVOKABLE void auto_connect_primary_bank();

        void update_status_line();

    signals:
        void status_line_changed();

    private:
        auth_service& auth_;
        connection_service& connections_;
        user_activity_monitor& activity_;
        notification_service& notifications_;
        account_service& accounts_;
        payment_service& payments_;
        exchange_advisor_service& advisor_;
        fx_service& fx_;
        clock_source& clock_;
        account_list_model* account_model_ {nullptr};
        recent_transactions_model* recent_txns_ {nullptr};
        recent_transactions_model* statement_txns_ {nullptr};
        transaction_filter_proxy* ledger_ {nullptr};
        beneficiary_model* beneficiary_model_ {nullptr};
        card_model* card_model_ {nullptr};
        card_service card_service_; // session-owned store; seeded via set_cards()
        analytics_service& analytics_;
        budget_service& budgets_;
        QString status_line_;

    signals:
        void display_currency_changed();
        void language_change_requested(const QString& locale);
        void accounts_changed();
        void net_worth_changed();
    };

} // namespace kmx
