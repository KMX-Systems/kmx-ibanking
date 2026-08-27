/// @file tests/tst_qml_surface.cpp
/// @brief Reflection guard over the QML-facing method, property, role and enum surface.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "connectors/kmx_connector.h"
#include "services/account_service.h"
#include "services/analytics_service.h"
#include "services/auth_service.h"
#include "services/budget_service.h"
#include "services/card_service.h"
#include "services/clock_source.h"
#include "services/connection_service.h"
#include "services/exchange_advisor_service.h"
#include "services/fx_service.h"
#include "services/notification_service.h"
#include "services/payment_service.h"
#include "services/user_activity_monitor.h"
#include "viewmodels/banking_session.h"
#include <QAbstractItemModel>
#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaObject>
#include <QRegularExpression>
#include <QtTest>

using namespace kmx;

// Guards the QML->C++ call surface: every method any QML page invokes must
// exist as a Q_INVOKABLE/slot on the exposed object. Regression net for the
// class of bug where a plain C++ method is silently uncallable from QML
// (connect_bank/refresh/disconnect/cancel_scheduled were all broken once).
class tst_QmlSurface final: public QObject
{
    Q_OBJECT

private slots:
    void every_qml_called_method_exists();
    void every_qml_read_property_exists();
    void qml_exposed_enum_keys_start_uppercase();
    void model_role_names_are_snake_case();

private:
    static bool has_method(const QObject* o, const QByteArray& name)
    {
        const QMetaObject* mo = o->metaObject();
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i)
            if (mo->method(i).name() == name)
                return true;
        return false;
    }

    static bool has_property(const QObject* o, const QByteArray& name) { return o->metaObject()->indexOfProperty(name.constData()) >= 0; }
};

void tst_QmlSurface::every_qml_called_method_exists()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0)));
    seed_world world; // empty world is fine for reflection
    fx_service fx;
    account_service accounts;
    notification_service notifications;
    connection_service connections(clock);
    payment_service payments(accounts, clock);
    exchange_advisor_service advisor(accounts, connections, fx);
    analytics_service analytics(accounts, fx, clock);
    budget_service budgets(accounts, fx, clock);
    budgets.set_notification_service(&notifications);
    auth_service auth(clock);
    user_activity_monitor monitor;
    card_service card_service;

    banking_session session(auth, connections, monitor, notifications, accounts, fx, payments, advisor, analytics, budgets, clock);

    // {object, method name} — mirrors every call site under qml/.
    const QList<QPair<const QObject*, QByteArray>> surface = {
        // banking_session
        {&session, "simulate_incoming_salary"},
        {&session, "simulate_fraud_alert"},
        {&session, "simulate_new_device_login"},
        {&session, "fx_shock"},
        {&session, "set_language"},
        {&session, "force_bt_session_expiry"},
        {&session, "auto_connect_primary_bank"},
        {&session, "is_valid_iban"},
        {&session, "copy_to_clipboard"},
        {&session, "export_ledger_csv"},
        {&session, "amend_transaction"},
        {&session, "month_in_out"},
        {&session, "account_iban"},
        {&session, "account_name"},
        {&session, "account_balance"},
        {&session, "account_currency_code"},
        {&session, "currency_for_iban"},
        {&session, "exchange_insights"},
        {&session, "budget_progress"},
        {&session, "bank_subtotals"},
        {&session, "linked_banks"},
        {&session, "create_virtual_card"},
        {&session, "set_card_frozen"},
        {&session, "set_card_online"},
        {&session, "set_card_contactless"},
        {&session, "set_card_limit"},

        // auth (LoginPage / OtpDialog / LockOverlay / header)
        {&auth, "authenticate"},
        {&auth, "verify_otp"},
        {&auth, "resend_otp"},
        {&auth, "lock"},
        {&auth, "unlock"},
        {&auth, "logout"},
        {&auth, "lockout_seconds_left"},

        // connections (ConnectionsPage / ConnectBankFlow / CardsPage)
        {&connections, "state"},
        {&connections, "last_sync_at"},
        {&connections, "last_error_text"},
        {&connections, "is_syncing"},
        {&connections, "expire_session_for_demo"},
        {&connections, "supports_virtual_cards"},
        {&connections, "link_bank"},
        {&connections, "refresh_bank"},
        {&connections, "disconnect_bank"},

        // payments (PaymentsPage / ExchangeHub)
        {&payments, "submit"},
        {&payments, "schedule_from_map"},
        {&payments, "scheduled_list"},
        {&payments, "cancel_scheduled"},
        {&payments, "run_due_now"},
        {&payments, "execute_exchange"},

        // advisor / analytics / budgets (ExchangeHub / Analytics / Dashboard)
        {&advisor, "advise"},
        {&advisor, "best_route"},
        {&analytics, "month_cashflow"},
        {&analytics, "category_breakdown"},
        {&analytics, "net_worth_series"},
        {&budgets, "budgets_for_month"},
        {&budgets, "progress"},
        {&budgets, "set_limit"},
        {&budgets, "limit_for"},

        // notifications (drawer / toasts)
        {&notifications, "post"},
        {&notifications, "mark_all_read"},
        {&notifications, "items_list"},

        // models
        {session.account_model(), "account_row_at"},
        {session.account_model(), "toggle_collapsed"},
        {session.account_model(), "refresh"},
        {session.beneficiaries(), "add"},
        {session.beneficiaries(), "remove"},
        {session.beneficiaries(), "toggle_favorite"},
        {session.beneficiaries(), "mark_used_by_iban"},
        {session.beneficiaries(), "get"},
    };

    for (const auto& entry: surface)
    {
        QVERIFY2(has_method(entry.first, entry.second),
                 qPrintable(QStringLiteral("%1 is not callable from QML on %2")
                                .arg(QString::fromLatin1(entry.second), entry.first->metaObject()->className())));
    }
}

// Properties QML binds to by name. A Q_PROPERTY that loses its NOTIFY, gets
// renamed, or never existed reads as `undefined` in a binding without warning
// -- which is how the dashboard's account list stayed empty for two phases.
void tst_QmlSurface::every_qml_read_property_exists()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0)));
    fx_service fx;
    account_service accounts;
    notification_service notifications;
    connection_service connections(clock);
    payment_service payments(accounts, clock);
    exchange_advisor_service advisor(accounts, connections, fx);
    analytics_service analytics(accounts, fx, clock);
    budget_service budgets(accounts, fx, clock);
    budgets.set_notification_service(&notifications);
    auth_service auth(clock);
    user_activity_monitor monitor;

    banking_session session(auth, connections, monitor, notifications, accounts, fx, payments, advisor, analytics, budgets, clock);

    // {object, property} — mirrors every `bank.<x>` binding under qml/.
    const QList<QPair<const QObject*, QByteArray>> properties = {
        {&session, "status_line"},
        {&session, "display_currency"},
        {&session, "net_worth_minor"},
        {&session, "account_model"},
        {&session, "recent_transactions"},
        {&session, "statement_transactions"},
        {&session, "ledger"},
        {&session, "auth"},
        {&session, "connections"},
        {&session, "activity"},
        {&session, "notifications"},
        {&session, "payments"},
        {&session, "advisor"},
        {&session, "analytics"},
        {&session, "budgets"},
        {&session, "beneficiaries"},
        {&session, "cards"},
        {&session, "bank_subtotals"},
        {&session, "exchange_insights"},
        {&session, "budget_progress"},
        {session.auth(), "state"},
        {session.auth(), "display_name"},
        {session.auth(), "idle_lock_seconds"},
        {session.activity(), "seconds_idle"},
        {session.notifications(), "unread_count"},
        {session.notifications(), "items_list"},
        {session.ledger(), "search_text"},
        {session.ledger(), "direction"},
        {session.ledger(), "from_date"},
        {session.ledger(), "to_date"},
        {session.ledger(), "bank_ids"},
        {session.ledger(), "categories"},
        {session.statement_transactions(), "account_filter"},
    };

    for (const auto& entry: properties)
    {
        QVERIFY2(has_property(entry.first, entry.second),
                 qPrintable(QStringLiteral("%1 is not a Q_PROPERTY on %2")
                                .arg(QString::fromLatin1(entry.second), entry.first->metaObject()->className())));
    }
}

// Qt only exposes Q_ENUM keys to QML when they begin with an uppercase letter;
// a lowercase key silently reads back as `undefined` in QML rather than failing
// loudly. These two enums are read by name from QML (AuthService.Active, ...),
// so their keys are pinned here against a well-meant snake_case rename.
void tst_QmlSurface::qml_exposed_enum_keys_start_uppercase()
{
    const QList<const QMetaObject*> qml_registered = {
        &auth_service::staticMetaObject,
        &connection_service::staticMetaObject,
    };

    for (const QMetaObject* mo: qml_registered)
    {
        for (int e = mo->enumeratorOffset(); e < mo->enumeratorCount(); ++e)
        {
            const QMetaEnum me = mo->enumerator(e);
            for (int k = 0; k < me.keyCount(); ++k)
            {
                const QString key = QString::fromLatin1(me.key(k));
                QVERIFY2(!key.isEmpty() && key.at(0).isUpper(),
                         qPrintable(QStringLiteral("%1::%2::%3 must start uppercase to be "
                                                   "readable from QML")
                                        .arg(QString::fromLatin1(mo->className()), QString::fromLatin1(me.name()), key)));
            }
        }
    }
}

// Role names are looked up from QML as strings, so a rename that misses a
// roleNames() entry fails silently. Pin the project's snake_case convention.
void tst_QmlSurface::model_role_names_are_snake_case()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0)));
    fx_service fx;
    account_service accounts;
    notification_service notifications;
    connection_service connections(clock);
    payment_service payments(accounts, clock);
    exchange_advisor_service advisor(accounts, connections, fx);
    analytics_service analytics(accounts, fx, clock);
    budget_service budgets(accounts, fx, clock);
    budgets.set_notification_service(&notifications);
    auth_service auth(clock);
    user_activity_monitor monitor;

    banking_session session(auth, connections, monitor, notifications, accounts, fx, payments, advisor, analytics, budgets, clock);

    const QList<const QAbstractItemModel*> models = {
        session.account_model(), session.recent_transactions(), session.statement_transactions(),
        session.ledger(),        session.beneficiaries(),       session.cards(),
    };

    static const QRegularExpression snake(QStringLiteral("^[a-z][a-z0-9_]*$"));
    for (const QAbstractItemModel* m: models)
    {
        const QHash<int, QByteArray> roles = m->roleNames();
        for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        {
            const QString name = QString::fromLatin1(it.value());
            // Qt's own display/decoration roles come along for the ride.
            if (name == QLatin1String("display") || name == QLatin1String("decoration") || name == QLatin1String("edit") ||
                name == QLatin1String("toolTip") || name == QLatin1String("statusTip") || name == QLatin1String("whatsThis"))
                continue;
            QVERIFY2(
                snake.match(name).hasMatch(),
                qPrintable(
                    QStringLiteral("role \"%1\" on %2 is not snake_case").arg(name, QString::fromLatin1(m->metaObject()->className()))));
        }
    }
}

QTEST_MAIN(tst_QmlSurface)
#include "tst_qml_surface.moc"
