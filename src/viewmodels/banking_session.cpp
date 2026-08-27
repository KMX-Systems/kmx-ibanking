/// @file src/viewmodels/banking_session.cpp
/// @brief Session wiring, display currency and the demo scenario triggers.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "banking_session.h"
#include "domain/budget.h"
#include "domain/iban.h"
#include "services/account_service.h"
#include "services/auth_service.h"
#include "services/connection_service.h"
#include "services/user_activity_monitor.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QTimer>
#include <algorithm>

namespace kmx
{

    banking_session::banking_session(auth_service& auth, connection_service& connections, user_activity_monitor& activity,
                                     notification_service& notifications, account_service& accounts, fx_service& fx,
                                     payment_service& payments, exchange_advisor_service& advisor, analytics_service& analytics,
                                     budget_service& budgets, clock_source& clock, QObject* parent):
        QObject(parent),
        auth_(auth),
        connections_(connections),
        activity_(activity),
        notifications_(notifications),
        accounts_(accounts),
        fx_(fx),
        payments_(payments),
        advisor_(advisor),
        analytics_(analytics),
        budgets_(budgets),
        clock_(clock),
        status_line_(tr("Ready"))
    {
        beneficiary_model_ = new beneficiary_model(this);
        card_model_ = new card_model(card_service_, this);

        // Keep the sidebar footer honest as links change state.
        connect(&connections_, &connection_service::state_changed, this, &banking_session::update_status_line);
        connect(&auth_, &auth_service::login_succeeded, this, &banking_session::update_status_line);

        // Scheduled payments ride the same 1 Hz heartbeat as the FX walk.
        auto* heartbeat = new QTimer(this);
        heartbeat->setInterval(1000);
        connect(heartbeat, &QTimer::timeout, this,
                [this]()
                {
                    payments_.tick();
                    const int previous = display_currency();
                    fx_.tick();
                    if (previous == display_currency())
                        emit net_worth_changed();
                });
        heartbeat->start();
        account_model_ = new account_list_model(accounts_, fx_, this);
        recent_txns_ = new recent_transactions_model(accounts_, this);
        statement_txns_ = new recent_transactions_model(accounts_, this);
        statement_txns_->set_limit(200);

        auto* ledger_source = new recent_transactions_model(accounts_, this);
        ledger_source->set_limit(0); // unlimited: the full ledger
        ledger_ = new transaction_filter_proxy(this);
        ledger_->setSourceModel(ledger_source);

        connect(&accounts_, &account_service::accounts_changed, this, &banking_session::net_worth_changed);
        connect(&accounts_, &account_service::accounts_changed, this, &banking_session::accounts_changed);
        // budget spend depends on transactions too; they land right after the
        // accounts in each sync, so re-emit once more for the property bindings.
        connect(&accounts_, &account_service::transactions_merged, this, &banking_session::accounts_changed);
        connect(&accounts_, &account_service::ledger_amended, this, &banking_session::accounts_changed);
    }

    int banking_session::display_currency() const
    {
        return QSettings().value(QStringLiteral("ui/display_currency"), 0).toInt();
    }

    void banking_session::set_display_currency(int currency_index)
    {
        if (currency_index == display_currency())
            return;
        QSettings().setValue(QStringLiteral("ui/display_currency"), currency_index);
        account_model_->refresh();
        emit display_currency_changed();
        emit net_worth_changed();
    }

    qint64 banking_session::net_worth_minor() const
    {
        qint64 total = 0;
        for (const auto& a: accounts_.accounts())
            total += fx_.convert(a.balance_minor, a.currency, static_cast<currency_code>(display_currency()));
        return total;
    }

    QVariantList banking_session::bank_subtotals() const
    {
        QVariantList out;
        const int disp = display_currency();

        for (int b = 0; b < bank_count; ++b)
        {
            qint64 total = 0;
            bool any = false;
            for (const auto& a: accounts_.accounts())
            {
                if (static_cast<int>(a.bank) != b)
                    continue;
                total += fx_.convert(a.balance_minor, a.currency, static_cast<currency_code>(disp));
                any = true;
            }
            if (!any)
                continue;
            out.append(QVariantMap {{"bank_id", b}, {"total_minor", total}});
        }
        return out;
    }

    QVariantList banking_session::budget_progress() const
    {
        return budgets_.progress();
    }

    QVariantList banking_session::month_in_out(int account_id) const
    {
        const QDateTime month_start(
            [this]
            {
                QDate d = clock_.now().date();
                d = QDate(d.year(), d.month(), 1);
                return QDateTime(d, QTime(0, 0));
            }());

        qint64 in = 0;
        qint64 out = 0;
        for (const auto& t: accounts_.transactions())
        {
            if (t.account_id != account_id || t.posted_at < month_start)
                continue;
            if (t.direction == txn_direction::credit)
                in += t.amount_minor;
            else
                out += t.amount_minor;
        }
        return {in, out};
    }

    const account* banking_session::find_account(int account_id) const
    {
        for (const auto& a: accounts_.accounts())
            if (a.id == account_id)
                return &a;
        return nullptr;
    }

    QString banking_session::account_iban(int account_id) const
    {
        const account* a = find_account(account_id);
        return a ? a->iban : QString();
    }

    QString banking_session::account_name(int account_id) const
    {
        const account* a = find_account(account_id);
        return a ? a->name : QString();
    }

    qint64 banking_session::account_balance(int account_id) const
    {
        const account* a = find_account(account_id);
        return a ? a->balance_minor : 0;
    }

    QString banking_session::account_currency_code(int account_id) const
    {
        const account* a = find_account(account_id);
        return a ? QLatin1String(to_code(a->currency)) : QStringLiteral("RON");
    }

    void banking_session::copy_to_clipboard(const QString& text)
    {
        QGuiApplication::clipboard()->setText(text);
    }

    bool banking_session::amend_transaction(const QString& reference_key, int new_category, const QString& note)
    {
        return accounts_.amend_transaction(reference_key, new_category, note);
    }

    QString banking_session::export_ledger_csv()
    {
        if (!ledger_)
            return {};
        return accounts_.export_csv(ledger_->visible_transactions());
    }

    QStringList banking_session::linked_banks() const
    {
        QStringList out;
        for (const auto id: {bank_id::kmx_bank, bank_id::banca_transilvania, bank_id::tbi_bank, bank_id::erste_bank})
            if (connections_.state(static_cast<int>(id)) != static_cast<int>(connection_service::link_state::Disconnected))
                out.append(QString::fromLatin1(bank_name(id)));
        return out;
    }

    void banking_session::auto_connect_primary_bank()
    {
        connections_.connect_bank(bank_id::kmx_bank, {QStringLiteral("ana.demo"), QStringLiteral("demo"), {}});
    }

    void banking_session::force_bt_session_expiry()
    {
        connections_.expire_session_for_demo(static_cast<int>(bank_id::banca_transilvania));
    }

    void banking_session::update_status_line()
    {
        const int linked = linked_banks().size();
        const QString next = tr("%1 bank%2 linked").arg(linked).arg(linked == 1 ? QString() : QStringLiteral("s"));
        if (next != status_line_)
        {
            status_line_ = next;
            emit status_line_changed();
        }
    }

    int banking_session::currency_for_iban(const QString& iban) const
    {
        for (const auto& a: accounts_.accounts())
            if (QString::compare(a.iban, iban, Qt::CaseInsensitive) == 0)
                return static_cast<int>(a.currency);
        return -1;
    }

    QVariantList banking_session::exchange_insights() const
    {
        QVariantList out;
        const int display = display_currency();

        // Idle cash in a currency different from the display one: would routing
        // the exchange elsewhere save more than 0.3%?
        for (const auto& a: accounts_.accounts())
        {
            if (static_cast<int>(a.currency) == display)
                continue;
            const qint64 amount = std::min<qint64>(a.available_minor(), 100'000'00); // cap for insight math
            if (amount < 10'000'00)
                continue;

            const QVariantList options = advisor_.advise(static_cast<int>(a.currency), display, amount);
            if (options.size() < 2)
                continue;

            const qint64 best = options.first().toMap()["resulting_minor"].toLongLong();
            const qint64 worst = options.last().toMap()["resulting_minor"].toLongLong();
            if (worst <= 0 || (best - worst) * 1000 <= worst * 3)
                continue; // below threshold

            const QVariantMap best_opt = options.first().toMap();
            const QVariantMap scaled_best = advisor_.best_route(static_cast<int>(a.currency), display, a.available_minor());

            QVariantMap card;
            card["account_id"] = a.id;
            card["account_name"] = a.name;
            card["bank_id"] = static_cast<int>(a.bank);
            card["from_code"] = QLatin1String(to_code(a.currency));
            card["to_code"] = QLatin1String(to_code(static_cast<currency_code>(display)));
            card["amount_minor"] = a.balance_minor;
            card["savings_minor"] = scaled_best.value("savings_vs_worst_minor").toLongLong();
            card["recommended_bank"] = best_opt["legs"].toList().first().toMap()["bank_id"];
            out.append(card);
            if (out.size() >= 2)
                break;
        }
        return out;
    }

    QVariantMap banking_session::create_virtual_card(int account_id, const QString& label, qint64 daily_limit_minor)
    {
        // Capability gate: only banks with virtual_cards may host virtual cards.
        int bank_id = -1;
        for (const auto& a: accounts_.accounts())
            if (a.id == account_id)
            {
                bank_id = static_cast<int>(a.bank);
                break;
            }

        if (!connections_.connector_capabilities(bank_id).virtual_cards)
            return {{"ok", false}, {"message", tr("This bank does not support virtual cards.")}};

        return card_service_.create_virtual(account_id, label, daily_limit_minor);
    }

    void banking_session::simulate_incoming_salary()
    {
        qint64 kmx_checking_id = -1;
        for (const auto& a: accounts_.accounts())
            if (a.bank == bank_id::kmx_bank && a.kind == account_kind::checking)
            {
                kmx_checking_id = a.id;
                break;
            }
        if (kmx_checking_id < 0)
        {
            notifications_.post(QStringLiteral("info"), QObject::tr("KMX Bank is not linked"),
                                QObject::tr("Connect it from the Connections page first."), QStringLiteral("connections"));
            return;
        }

        QString ref;
        accounts_.post_local_transaction(kmx_checking_id, txn_direction::credit, QStringLiteral("NordTech SRL salary"), {}, 850'000'00,
                                         QStringLiteral("Demo salary"), &ref);
        notifications_.post(QStringLiteral("success"), QObject::tr("Salary received"), QObject::tr("+8 500,00 RON from NordTech SRL"),
                            QStringLiteral("transactions"));
    }

    void banking_session::simulate_fraud_alert()
    {
        notifications_.post(QStringLiteral("critical"), QObject::tr("Suspicious card activity"),
                            QObject::tr("Card used in another country. "
                                        "Review and freeze it if this wasn't you."),
                            QStringLiteral("cards"));
    }

    void banking_session::simulate_new_device_login()
    {
        notifications_.post(QStringLiteral("info"), QObject::tr("New device sign-in"),
                            QObject::tr("Firefox on Linux · Cluj-Napoca · just now"), QStringLiteral("settings"));
    }

    bool banking_session::is_valid_iban(const QString& iban) const
    {
        return kmx::is_valid_iban(iban); // qualified: the member would recurse
    }

    void banking_session::set_budgets_and_beneficiaries(const QVector<budget>& budgets, QVector<beneficiary> beneficiaries)
    {
        budgets_.set_budgets(budgets);
        if (beneficiary_model_)
            beneficiary_model_->set_beneficiaries(std::move(beneficiaries));
    }

} // namespace kmx
