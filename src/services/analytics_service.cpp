/// @file src/services/analytics_service.cpp
/// @brief Cashflow, category breakdown and net-worth series computation.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "analytics_service.h"
#include "services/account_service.h"
#include "services/clock_source.h"
#include "services/fx_service.h"
#include <QLocale>
#include <algorithm>

namespace kmx
{

    analytics_service::analytics_service(account_service& accounts, fx_service& fx, clock_source& clock, QObject* parent):
        QObject(parent),
        accounts_(accounts),
        fx_(fx),
        clock_(clock)
    {
    }

    QDateTime analytics_service::month_start(int year, int month) const
    {
        return QDateTime(QDate(year, month, 1), QTime(0, 0));
    }

    QString analytics_service::month_label(int year, int month)
    {
        return QLocale::c().standaloneMonthName(month, QLocale::ShortFormat);
    }

    QVariantList analytics_service::month_cashflow(int months) const
    {
        QVariantList out;
        if (months <= 0)
            months = 6;
        months = std::min(months, 24);

        const QDate today = clock_.now().date();
        // Buckets oldest -> newest, ending with the current month.
        for (int back = months - 1; back >= 0; --back)
        {
            QDate bucket_start(today.year(), today.month(), 1);
            bucket_start = bucket_start.addMonths(-back);
            const QDateTime from(bucket_start, QTime(0, 0));
            const QDateTime to = from.addMonths(1);

            qint64 income = 0;
            qint64 expense = 0;
            for (const auto& t: accounts_.transactions())
            {
                if (t.posted_at < from || t.posted_at >= to)
                    continue;
                if (t.direction == txn_direction::credit)
                    income += fx_.convert(t.amount_minor, t.currency, currency_code::ron);
                else
                    expense += fx_.convert(t.amount_minor, t.currency, currency_code::ron);
            }

            QVariantMap row;
            row["label"] = month_label(bucket_start.year(), bucket_start.month());
            row["year"] = bucket_start.year();
            row["month"] = bucket_start.month();
            row["income_minor"] = income;
            row["expense_minor"] = expense;
            out.append(row);
        }
        return out;
    }

    QVariantList analytics_service::category_breakdown(int year, int month) const
    {
        const QDateTime from = month_start(year, month);
        const QDateTime to = from.addMonths(1);

        QHash<int, qint64> spent; // category -> RON minor
        for (const auto& t: accounts_.transactions())
        {
            if (t.direction != txn_direction::debit || t.posted_at < from || t.posted_at >= to)
                continue;
            spent[static_cast<int>(t.category)] += fx_.convert(t.amount_minor, t.currency, currency_code::ron);
        }

        QVariantList rows;
        for (auto it = spent.constBegin(); it != spent.constEnd(); ++it)
            rows.append(QVariantMap {{"category", it.key()}, {"spent_minor", it.value()}});
        std::sort(rows.begin(), rows.end(), [](const QVariant& a, const QVariant& b)
                  { return a.toMap()["spent_minor"].toLongLong() > b.toMap()["spent_minor"].toLongLong(); });
        return rows;
    }

    QVariantList analytics_service::net_worth_series(int points) const
    {
        QVariantList out;
        points = std::clamp(points, 2, 24);

        const QVector<transaction>& txns = accounts_.transactions();
        const QVector<account> accounts = accounts_.accounts();

        qint64 current = 0;
        for (const auto& a: accounts)
            current += fx_.convert(a.balance_minor, a.currency, currency_code::ron);

        // Reconstructed history: balance(cutoff) =
        //   currentTotal − Σ(signed amounts booked after that cutoff).
        // Current mids apply to historical amounts (documented approximation).
        const QDate today = clock_.now().date();

        QVariantMap now_row;
        now_row["label"] = QStringLiteral("now");
        now_row["total_minor"] = current;
        out.append(now_row);

        QDateTime cutoff(QDate(today.year(), today.month(), 1), QTime(0, 0));
        cutoff = cutoff.addSecs(-1); // last instant of previous month

        for (int i = 0; i < points - 1; ++i)
        {
            qint64 newer = 0;
            for (const auto& t: txns)
                if (t.posted_at > cutoff)
                    newer += fx_.convert(t.signed_amount_minor(), t.currency, currency_code::ron);

            QVariantMap row;
            row["label"] = month_label(cutoff.date().year(), cutoff.date().month());
            row["total_minor"] = current - newer;
            out.prepend(row);

            cutoff = cutoff.addMonths(-1);
        }
        return out;
    }

} // namespace kmx
