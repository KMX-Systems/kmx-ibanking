/// @file src/services/budget_service.cpp
/// @brief Budget spend tracking and warning dispatch.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "budget_service.h"
#include "services/account_service.h"
#include "services/clock_source.h"
#include "services/fx_service.h"
#include "services/normalization.h"
#include "services/notification_service.h"
#include <algorithm>

namespace kmx
{

    budget_service::budget_service(account_service& accounts, fx_service& fx, clock_source& clock, QObject* parent):
        QObject(parent),
        accounts_(accounts),
        fx_(fx),
        clock_(clock)
    {
    }

    QByteArray budget_service::month_key(int category) const
    {
        const QDate d = clock_.now().date();
        return QStringLiteral("%1-%2:%3").arg(d.year()).arg(d.month(), 2, 10, QLatin1Char('0')).arg(category).toUtf8();
    }

    qint64 budget_service::spent_in_month(int category, const QDateTime& from, const QDateTime& to) const
    {
        qint64 sum = 0;
        for (const auto& t: accounts_.transactions())
        {
            if (t.direction != txn_direction::debit || static_cast<int>(t.category) != category || t.posted_at < from || t.posted_at >= to)
                continue;
            sum += fx_.convert(t.amount_minor, t.currency, currency_code::ron);
        }
        return sum;
    }

    QVariantList budget_service::budgets_for_month(int year, int month) const
    {
        const QDateTime from(QDate(year, month, 1), QTime(0, 0));
        const QDateTime to = from.addMonths(1);

        QVariantList rows;
        for (const auto& b: budgets_)
        {
            QVariantMap row;
            row["category"] = static_cast<int>(b.category);
            row["limit_minor"] = b.monthly_limit_minor;
            row["spent_minor"] = spent_in_month(static_cast<int>(b.category), from, to);
            rows.append(row);
        }

        std::sort(rows.begin(), rows.end(),
                  [](const QVariant& a, const QVariant& b)
                  {
                      const auto& ma = a.toMap();
                      const auto& mb = b.toMap();
                      const double ua = ma["spent_minor"].toDouble() / std::max<qint64>(1, ma["limit_minor"].toLongLong());
                      const double ub = mb["spent_minor"].toDouble() / std::max<qint64>(1, mb["limit_minor"].toLongLong());
                      return ua > ub;
                  });
        return rows;
    }

    QVariantList budget_service::progress() const
    {
        const QDate today = clock_.now().date();
        QVariantList all = budgets_for_month(today.year(), today.month());
        while (all.size() > 4)
            all.removeLast();
        return all;
    }

    bool budget_service::set_limit(int category, qint64 limit_minor_ron)
    {
        if (limit_minor_ron < 0)
            return false;
        for (auto& b: budgets_)
        {
            if (static_cast<int>(b.category) == category)
            {
                b.monthly_limit_minor = limit_minor_ron;
                return true;
            }
        }
        // Unknown category: append an envelope on demand.
        budget b;
        b.category = static_cast<txn_category>(category);
        b.monthly_limit_minor = limit_minor_ron;
        b.limit_currency = currency_code::ron;
        budgets_.append(b);
        return true;
    }

    qint64 budget_service::limit_for(int category) const
    {
        for (const auto& b: budgets_)
            if (static_cast<int>(b.category) == category)
                return b.monthly_limit_minor;
        return 0;
    }

    void budget_service::check_warnings()
    {
        if (!notifications_)
            return;

        const QDate today = clock_.now().date();
        const QDateTime from(QDate(today.year(), today.month(), 1), QTime(0, 0));
        const QDateTime to = from.addMonths(1);

        for (const auto& b: budgets_)
        {
            if (b.monthly_limit_minor <= 0)
                continue;

            const QByteArray key = month_key(static_cast<int>(b.category));
            if (fired_warnings_.contains(key))
                continue;

            const qint64 spent = spent_in_month(static_cast<int>(b.category), from, to);
            if (spent > b.monthly_limit_minor)
            {
                fired_warnings_.insert(key); // once per category per month
                notifications_->post(QStringLiteral("warning"), QObject::tr("Budget exceeded"),
                                     QObject::tr("%1 spending is over budget (%2 of %3 RON).")
                                         .arg(QLatin1String(category_label(b.category)))
                                         .arg(spent / 100.0, 0, 'f', 0)
                                         .arg(b.monthly_limit_minor / 100.0, 0, 'f', 0),
                                     QStringLiteral("analytics"));
            }
        }
    }

} // namespace kmx
