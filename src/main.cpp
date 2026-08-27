/// @file src/main.cpp
/// @brief Application entry point: wires the banking core and loads the QML shell.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "app/ui_config.h"
#include "connectors/bt_connector.h"
#include "connectors/erste_connector.h"
#include "connectors/kmx_connector.h"
#include "connectors/tbi_connector.h"
#include "domain/seed_world.h"
#include "services/account_service.h"
#include "services/analytics_service.h"
#include "services/auth_service.h"
#include "services/budget_service.h"
#include "services/clock_source.h"
#include "services/connection_service.h"
#include "services/exchange_advisor_service.h"
#include "services/fx_service.h"
#include "services/notification_service.h"
#include "services/payment_service.h"
#include "services/user_activity_monitor.h"
#include "viewmodels/banking_session.h"
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>
#include <memory>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("KmxBank");

    // Expose service types to QML for enum access (bank.auth.Active etc).
    // Instances are owned by banking_session; QML never creates them.
    // QML type names must begin with an uppercase letter, so the names exposed
    // to QML stay PascalCase even though the C++ types are snake_case. They are
    // registered only so QML can reach the Q_ENUM keys (AuthService.Active, ...).
    qmlRegisterUncreatableType<kmx::auth_service>("KmxBank", 1, 0, "AuthService", QStringLiteral("Owned by banking_session"));
    qmlRegisterUncreatableType<kmx::connection_service>("KmxBank", 1, 0, "ConnectionService", QStringLiteral("Owned by banking_session"));
    app.setOrganizationName("KMX");
    app.setApplicationVersion("0.1");

    // Which shell to build: --ui=desktop|mobile|auto, KMX_UI, or the remembered
    // choice. Must run after the org/app names above, since it persists via
    // QSettings. Exposed to QML so layouts can read the emulated device.
    kmx::ui_config ui;
    ui.resolve(app);
    qmlRegisterSingletonInstance("KmxBank", 1, 0, "UiConfig", &ui);

    QFont ui_font = app.font();
    ui_font.setPointSizeF(8.5);
    app.setFont(ui_font);

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_VARIANT"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_THEME"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark");

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Material"));

    // Banking core: deterministic world + link lifecycle + auth/session.
    system_clock clock;
    const kmx::seed_world world = kmx::generate_seed_world(clock);
    kmx::account_service accounts;
    kmx::notification_service notifications;
    kmx::connection_service connections(clock);

    connections.set_account_service(&accounts);
    connections.set_notification_service(&notifications);

    // Per-bank personas: sync latency theater + staggered auto-sync cadence
    // (plan §2). Erste's auto-sync respects its own 5-minute rate limit.
    auto kmx_bank = std::make_unique<kmx::kmx_connector>(world, clock);
    connections.register_connector(std::move(kmx_bank));
    connections.register_connector(std::make_unique<kmx::bt_connector>(world, clock));
    connections.register_connector(std::make_unique<kmx::tbi_connector>(world, clock));
    connections.register_connector(std::make_unique<kmx::erste_connector>(world, clock));

    connections.set_sync_delay(kmx::bank_id::kmx_bank, std::chrono::milliseconds(150));
    connections.set_sync_delay(kmx::bank_id::banca_transilvania, std::chrono::milliseconds(2500));
    connections.set_sync_delay(kmx::bank_id::tbi_bank, std::chrono::milliseconds(120));
    connections.set_sync_delay(kmx::bank_id::erste_bank, std::chrono::milliseconds(700));

    connections.set_auto_sync_interval(kmx::bank_id::kmx_bank, 20'000);
    connections.set_auto_sync_interval(kmx::bank_id::banca_transilvania, 45'000);
    connections.set_auto_sync_interval(kmx::bank_id::tbi_bank, 25'000);
    connections.set_auto_sync_interval(kmx::bank_id::erste_bank, 310'000);

    kmx::fx_service fx;
    kmx::payment_service payments(accounts, clock);

    // Cards live on the session; enforcement hooks into payments here.
    // (set_card_service called after session construction below.)
    kmx::exchange_advisor_service advisor(accounts, connections, fx);
    payments.set_advisor(&advisor);

    kmx::analytics_service analytics(accounts, fx, clock);
    kmx::budget_service budgets(accounts, fx, clock);
    budgets.set_notification_service(&notifications);

    // Capability resolver for scheduled payments (plan §2 matrix).
    payments.set_capability_resolver([&connections](int bank_id)
                                     { return connections.connector_capabilities(bank_id).scheduled_payments; });

    kmx::auth_service auth(clock);
    kmx::user_activity_monitor activity(&app);
    activity.install();
    kmx::banking_session session(auth, connections, activity, notifications, accounts, fx, payments, advisor, analytics, budgets, clock);
    session.set_budgets_and_beneficiaries(world.budgets, world.beneficiaries);
    session.set_cards(world.cards);
    payments.set_card_service(session.cards_service());
    session.update_status_line();

    QQmlApplicationEngine qml_engine;
    qml_engine.setInitialProperties({
        {QStringLiteral("bank"), QVariant::fromValue(&session)},
    });

    // Language switching: Settings page calls bank.set_language(locale).
    auto* translator = new QTranslator(&app);
    QObject::connect(&session, &kmx::banking_session::language_change_requested, &app,
                     [translator, &qml_engine](const QString& locale)
                     {
                         QCoreApplication::removeTranslator(translator);
                         if (locale != QLatin1String("en"))
                         {
                             const QString path = QStringLiteral(":/i18n/kmxbank_") + locale + QStringLiteral(".qm");
                             if (translator->load(path))
                                 QCoreApplication::installTranslator(translator);
                         }
                         qml_engine.retranslate();
                     });

    QObject::connect(
        &qml_engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    qml_engine.loadFromModule("KmxBank", ui.mobile() ? "MobileShell" : "MainShell");

    // Headless smoke/CI hook: KMX_AUTOLOGIN=1 logs in, walks every route and
    // captures a screenshot at the end. KMX_ROUTES narrows the walk, KMX_SHOT
    // picks the output file (default /workspace/screenshot.png), KMX_LINK_BANKS
    // links every bank first and KMX_SHOT_DELAY delays the grab.
    if (!qEnvironmentVariableIsEmpty("KMX_AUTOLOGIN"))
    {
        const auto verify_otp_on_issue = [&auth, &app](const QString& code)
        { QTimer::singleShot(10, &app, [&auth, code] { auth.verify_otp(code); }); };
        QObject::connect(&auth, &kmx::auth_service::otp_issued, &app, verify_otp_on_issue);
        QTimer::singleShot(10, &app, [&auth] { auth.authenticate(QStringLiteral("smoke"), QStringLiteral("smoke")); });

        QObject::connect(
            &auth, &kmx::auth_service::login_succeeded, &app,
            [&qml_engine, &app, &connections]
            {
                // KMX_LINK_BANKS=1 links the other three banks up front, so the walk
                // sees the aggregated (multi-bank) state instead of KMX alone.
                if (!qEnvironmentVariableIsEmpty("KMX_LINK_BANKS"))
                    for (const auto bank: {kmx::bank_id::banca_transilvania, kmx::bank_id::tbi_bank, kmx::bank_id::erste_bank})
                        connections.connect_bank(bank, {QStringLiteral("smoke"), QStringLiteral("smoke"), QStringLiteral("000000")});

                const QStringList keys =
                    qEnvironmentVariable("KMX_ROUTES").isEmpty() ?
                        QStringList {QStringLiteral("accounts"),    QStringLiteral("transactions"), QStringLiteral("payments"),
                                     QStringLiteral("exchange"),    QStringLiteral("cards"),        QStringLiteral("analytics"),
                                     QStringLiteral("connections"), QStringLiteral("settings"),     QStringLiteral("dashboard")} :
                        qEnvironmentVariable("KMX_ROUTES").split(u',');

                auto* walker = new QTimer(&app);
                walker->setInterval(900);
                auto step = std::make_shared<int>(0);
                QObject::connect(
                    walker, &QTimer::timeout, walker,
                    [walker, &app, &qml_engine, keys, step]()
                    {
                        if (qml_engine.rootObjects().isEmpty() || *step >= keys.size())
                        {
                            walker->stop();
                            walker->deleteLater();
                            // KMX_SHOT_DELAY buys time for slow connectors (BT syncs in 2.5 s).
                            QTimer::singleShot(
                                qEnvironmentVariableIntValue("KMX_SHOT_DELAY") > 0 ? qEnvironmentVariableIntValue("KMX_SHOT_DELAY") : 1500,
                                &app,
                                [&app]
                                {
                                    if (auto* win = qobject_cast<QQuickWindow*>(QGuiApplication::focusWindow()))
                                    {
                                        const QString path = qEnvironmentVariable("KMX_SHOT", QStringLiteral("/workspace/screenshot.png"));
                                        const QImage img = win->grabWindow();
                                        img.save(path);
                                        qInfo() << "screenshot saved:" << path << img.size();
                                    }
                                    else
                                    {
                                        qWarning() << "no focused window to grab";
                                    }
                                });
                            return;
                        }
                        const QVariant key = keys[(*step)++];
                        QVariant result;
                        QMetaObject::invokeMethod(qml_engine.rootObjects().first(), "open_route", Q_RETURN_ARG(QVariant, result),
                                                  Q_ARG(QVariant, key));
                    });
                walker->start();
            });
    }

    return app.exec();
}
