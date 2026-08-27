/// @file src/services/card_service.h
/// @brief Card management: freeze, toggles, limits and virtual cards.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QObject>
    #include <QVector>
    #include <functional>
#endif
#include "domain/bank.h"
#include "domain/card.h"
namespace kmx
{

    class account_service;

    /// @brief card management (plan §Phase 8):
    /// @details freeze/toggles/limits/virtual creation. Enforcement lives in payment_service via set_card_service().
    class card_service: public QObject
    {
        Q_OBJECT
    public:
        explicit card_service(QObject* parent = nullptr);

        void set_cards(QVector<card> cards) { cards_ = std::move(cards); }
        void set_capability_resolver(std::function<bool(int bank_id)> resolver) { capabilities_ = std::move(resolver); }

        QVector<card> cards() const { return cards_; }
        const card* card_by_id(qint64 id) const;

        // True when the given account has a frozen card attached.
        Q_INVOKABLE bool account_frozen(qint64 account_id) const;
        // Today's card limit for the account, 0 when unlimited/no card.
        Q_INVOKABLE qint64 daily_limit_for_account(qint64 account_id) const;

        Q_INVOKABLE bool set_frozen(qint64 card_id, bool frozen);
        Q_INVOKABLE bool set_online_payments(qint64 card_id, bool on);
        Q_INVOKABLE bool set_contactless(qint64 card_id, bool on);
        // Limit in minor units; 0 disables the cap. Fails with a message when invalid.
        Q_INVOKABLE QVariantMap set_daily_limit(qint64 card_id, qint64 limit_minor);

        // Virtual cards only where the bank supports them (plan §2 matrix).
        Q_INVOKABLE QVariantMap create_virtual(int account_id, const QString& label, qint64 daily_limit_minor);

    signals:
        void cards_changed();

    private:
        card* find(qint64 id);

        QVector<card> cards_;
        std::function<bool(int)> capabilities_ {[](int) { return false; }};
    };

} // namespace kmx
