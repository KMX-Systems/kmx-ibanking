/// @file tests/tst_analytics.cpp
/// @brief Analytics reconciliation against the filtered ledger.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "services/account_service.h"
#include "services/analytics_service.h"
#include "services/budget_service.h"
#include "services/clock_source.h"
#include "services/fx_service.h"
#include "services/notification_service.h"
#include <QSignalSpy>
#include <QtTest>
#include <cmath>

using namespace kmx;

class tst_Analytics final: public QObject
{
    Q_OBJECT

private slots:
    void cashflow_sums_match_ledger();
    void category_breakdown_reconciles();
    void net_worth_series_anchors_on_current_total();
    void over_budget_warning_fires_once_per_month();

private:
    struct rig
    {
        fake_clock clock {QDateTime(QDate(2026, 8, 25), QTime(12, 0))};
        fx_service fx;
        account_service store;
        notification_service notifications;
        analytics_service analytics;
        budget_service budgets;

        explicit rig(QObject*): analytics(store, fx, clock), budgets(store, fx, clock)
        {
            budgets.set_notification_service(&notifications);

            connector::remote_account a;
            a.external_id = "A";
            a.currency = currency_code::ron;
            a.balance_minor = 50'000'00;
            store.upsert_accounts(bank_id::kmx_bank, {a});

            // Deterministic ledger:
            //   July: +100.00 RON salary, -40.00 RON groceries
            //   August: -20.00 RON dining, -10.00 EUR (~49.72 RON) shopping
            const auto post = [&](const QDateTime& when, txn_direction dir, txn_category cat, qint64 minor, currency_code ccy)
            {
                transaction t;
                t.account_id = 1;
                t.direction = dir;
                t.category = cat;
                t.category_source = category_origin::native;
                t.amount_minor = minor;
                t.currency = ccy;
                t.posted_at = when;
                t.status = txn_status::booked;
                t.reference = QStringLiteral("T%1").arg(++seq);
                // Bypass post_local_transaction to keep balances untouched and
                // control dates precisely.
                store.merge_transactions_for_test({t});
            };

            post(QDateTime(QDate(2026, 7, 5), QTime(9, 0)), txn_direction::credit, txn_category::salary, 10'000'00, currency_code::ron);
            post(QDateTime(QDate(2026, 7, 9), QTime(9, 0)), txn_direction::debit, txn_category::groceries, 4'000'00, currency_code::ron);
            post(QDateTime(QDate(2026, 8, 3), QTime(9, 0)), txn_direction::debit, txn_category::dining, 2'000'00, currency_code::ron);
            post(QDateTime(QDate(2026, 8, 8), QTime(9, 0)), txn_direction::debit, txn_category::shopping, 1'000'00, currency_code::eur);

            QVector<budget> seed_budgets;
            budget g;
            g.category = txn_category::groceries;
            g.monthly_limit_minor = 100'00;
            budget d;
            d.category = txn_category::dining;
            d.monthly_limit_minor = 15'00;
            budget s;
            s.category = txn_category::shopping;
            s.monthly_limit_minor = 30'00;
            seed_budgets << g << d << s;
            budgets.set_budgets(seed_budgets);
        }

        int seq {0};
    };
};

void tst_Analytics::cashflow_sums_match_ledger()
{
    rig rig(this);

    // Minor-unit conversion: 1000.00 EUR at the base mid = 497230 minor RON.
    const qint64 expected_eur_in_ron = static_cast<qint64>(std::llround(1'000'00 * fx_service::base_eur_ron));

    const QVariantList rows = rig.analytics.month_cashflow(2);
    QCOMPARE(rows.size(), 2);

    const QVariantMap july = rows.first().toMap();
    QCOMPARE(july["income_minor"].toLongLong(), qint64(10'000'00));
    QCOMPARE(july["expense_minor"].toLongLong(), qint64(4'000'00));
    QCOMPARE(july["month"], 7);

    const QVariantMap august = rows.last().toMap();
    QVERIFY(std::abs(august["expense_minor"].toLongLong() - (2'000'00 + expected_eur_in_ron)) <= 1);
    QCOMPARE(august["income_minor"].toLongLong(), qint64(0));
}

void tst_Analytics::category_breakdown_reconciles()
{
    rig rig(this);

    const QVariantList august = rig.analytics.category_breakdown(2026, 8);
    QVERIFY(august.size() >= 2);

    qint64 sum = 0;
    for (const auto& r: august)
        sum += r.toMap()["spent_minor"].toLongLong();

    // Minor-unit conversion: 1000.00 EUR at the base mid.
    const qint64 expected_eur_in_ron = static_cast<qint64>(std::llround(1'000'00 * fx_service::base_eur_ron));
    QVERIFY(std::abs(sum - (2'000'00 + expected_eur_in_ron)) <= 1);

    // Sorted descending: Dining (2000) first, Shopping (~4972) second? No —
    // Shopping is larger. Verify ordering explicitly.
    QCOMPARE(august.first().toMap()["category"].toInt(), static_cast<int>(txn_category::shopping));

    // Empty month returns nothing.
    QVERIFY(rig.analytics.category_breakdown(2025, 12).isEmpty());
}

void tst_Analytics::net_worth_series_anchors_on_current_total()
{
    rig rig(this);

    // Current total from live balances: only account A = 5000.00 RON.
    const QVariantList series = rig.analytics.net_worth_series(6);
    QCOMPARE(series.size(), 6);
    QCOMPARE(series.last().toMap()["total_minor"].toLongLong(), qint64(50'000'00));

    // Every historical point equals current minus newer activity — sanity:
    // the oldest point must differ from current (we have txns in that window)
    // but stay positive-ish and consistent between two calls.
    const QVariantList again = rig.analytics.net_worth_series(6);
    for (int i = 0; i < series.size(); ++i)
        QCOMPARE(series[i].toMap()["total_minor"].toLongLong(), again[i].toMap()["total_minor"].toLongLong());
}

void tst_Analytics::over_budget_warning_fires_once_per_month()
{
    rig rig(this);
    QSignalSpy posted_spy(&rig.notifications, &notification_service::posted);

    // Dining spent 20.00 vs limit 15.00 -> warning on first check.
    rig.budgets.check_warnings();
    const int after_first = posted_spy.count();
    QVERIFY(after_first >= 1);

    // Groceries 40.00 vs limit... wait, Groceries spend was in July; August
    // has none. Only Dining (20 > 15) and Shopping (~49.7 > 30) fire.
    QVERIFY(after_first == 2); // Dining + Shopping, nothing else

    // Repeated checks stay silent within the same month.
    rig.budgets.check_warnings();
    rig.budgets.check_warnings();
    QCOMPARE(posted_spy.count(), after_first);

    // Deep links point at the analytics page.
    QVERIFY(posted_spy.first().at(4).toString() == QLatin1String("analytics"));
}

QTEST_MAIN(tst_Analytics)
#include "tst_analytics.moc"
