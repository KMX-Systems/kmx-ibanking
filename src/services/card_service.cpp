/// @file src/services/card_service.cpp
/// @brief Card state changes and virtual card creation.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "card_service.h"
#include <QRandomGenerator>
#include <QVariant>
#include <algorithm>

namespace kmx
{

    namespace detail
    {
        QString random_pan(card_network net)
        {
            const quint32 prefix = net == card_network::visa ? 4539 : 5412;
            auto* rng = QRandomGenerator::global();
            quint64 body = 0;
            for (int i = 0; i < 3; ++i)
                body = body * 10'000 + rng->bounded(10'000u);
            return QStringLiteral("%1%2").arg(prefix).arg(body % 100'000'000'000ULL, 12, 10, QLatin1Char('0'));
        }
    } // namespace detail
    card_service::card_service(QObject* parent): QObject(parent)
    {
    }

    const card* card_service::card_by_id(qint64 id) const
    {
        for (const auto& c: cards_)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    card* card_service::find(qint64 id)
    {
        for (auto& c: cards_)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    bool card_service::account_frozen(qint64 account_id) const
    {
        for (const auto& c: cards_)
            if (c.account_id == account_id && c.frozen)
                return true;
        return false;
    }

    qint64 card_service::daily_limit_for_account(qint64 account_id) const
    {
        qint64 lowest = 0; // 0 = no cap; several cards -> strictest active one wins
        bool any = false;
        for (const auto& c: cards_)
        {
            if (c.account_id != account_id || c.frozen || c.daily_limit_minor <= 0)
                continue;
            if (!any || c.daily_limit_minor < lowest)
            {
                lowest = c.daily_limit_minor;
                any = true;
            }
        }
        return any ? lowest : 0;
    }

    bool card_service::set_frozen(qint64 card_id, bool frozen)
    {
        card* c = find(card_id);
        if (!c || c->frozen == frozen)
            return false;
        c->frozen = frozen;
        emit cards_changed();
        return true;
    }

    bool card_service::set_online_payments(qint64 card_id, bool on)
    {
        card* c = find(card_id);
        if (!c || c->online_payments == on)
            return false;
        c->online_payments = on;
        emit cards_changed();
        return true;
    }

    bool card_service::set_contactless(qint64 card_id, bool on)
    {
        card* c = find(card_id);
        if (!c || c->contactless == on)
            return false;
        c->contactless = on;
        emit cards_changed();
        return true;
    }

    QVariantMap card_service::set_daily_limit(qint64 card_id, qint64 limit_minor)
    {
        card* c = find(card_id);
        if (!c)
            return {{"ok", false}, {"message", QStringLiteral("Unknown card.")}};
        if (limit_minor < 0)
            return {{"ok", false}, {"message", QStringLiteral("Limit must be positive.")}};
        if (limit_minor > 0 && limit_minor < 1'00)
            return {{"ok", false}, {"message", QStringLiteral("Limit must be at least 1.00 or zero to disable.")}};
        c->daily_limit_minor = limit_minor;
        emit cards_changed();
        return {{"ok", true}};
    }

    QVariantMap card_service::create_virtual(int account_id, const QString& label, qint64 daily_limit_minor)
    {
        // Capability gating happens upstream: the session only exposes accounts
        // of banks whose matrix has virtual_cards = true.
        card c;
        qint64 next_id = 1;
        for (const auto& existing: cards_)
            next_id = std::max(next_id, existing.id + 1);
        c.id = next_id;
        c.account_id = account_id;
        c.label = label.isEmpty() ? QStringLiteral("Virtual card") : label.simplified();
        c.network = QRandomGenerator::global()->bounded(2) == 0 ? card_network::visa : card_network::mastercard;
        c.full_pan = detail::random_pan(c.network);
        c.masked_pan = c.full_pan.left(4) + QStringLiteral(" •••• •••• ") + c.full_pan.right(4);
        c.cvv = QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(1000), 3, 10, QLatin1Char('0'));
        c.expiry_mmyy = QStringLiteral("12/29");
        c.holder_name = QStringLiteral("Ana Dumitrescu");
        c.is_virtual = true;
        c.daily_limit_minor = daily_limit_minor;

        cards_.append(c);
        emit cards_changed();
        return {{"ok", true}, {"id", c.id}};
    }

} // namespace kmx
