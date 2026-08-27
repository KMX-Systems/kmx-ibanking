/// @file src/services/exchange_advisor_service.h
/// @brief Smart exchange routing across every connected bank's FX desk.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QObject>
    #include <QVariantList>
    #include <QVector>
#endif
#include "domain/bank.h"
#include "domain/currency.h"
namespace kmx
{

    class account_service;
    class connection_service;
    class fx_service;

    /// @brief Smart exchange routing (plan §6):
    /// @details given a currency pair and amount, compare what every connected bank's FX desk would actually pay out, across direct and
    /// two-leg routes, and rank by resulting amount. Guarantees: * exact minor-unit arithmetic per leg (out = round(in·mid·(1-spread)) −
    /// fee) * a venue qualifies only when the user funds it (an account there holds enough of the source currency) AND holds a
    /// destination-currency account for execution feasibility * disconnected venues never rank; rate-limited ones stay listed but flagged *
    /// deterministic ordering: result desc, then fee asc, legs asc, venue order
    struct exchange_leg
    {
        int bank_id {0};
        currency_code from {currency_code::ron};
        currency_code to {currency_code::eur};
        qint64 in_minor {0};
        qint64 out_minor {0};
        double applied_rate {0.0};  // mid × (1 − spread), display only
        qint64 fee_minor {0};       // in `to` currency units
        int source_account_id {-1}; // funded account used on this venue
    };

    struct exchange_option
    {
        QVector<exchange_leg> legs;
        qint64 resulting_minor {0};        // in final currency
        qint64 savings_vs_worst_minor {0}; // vs the worst viable option (0 if only one)
        QString explanation_key;           // ROUTE_DIRECT_NO_FEE | ROUTE_LOWEST_SPREAD |
                                           // ROUTE_TWO_LEG_VIA | ROUTE_SAVES_VS_DEFAULT
        QVariantList explanation_args;
        bool rate_limited_venue {false};
        QString rate_limit_note;
    };

    class exchange_advisor_service: public QObject
    {
        Q_OBJECT
    public:
        exchange_advisor_service(account_service& accounts, connection_service& connections, fx_service& fx, QObject* parent = nullptr);

        // Amount is expressed in the SOURCE currency. Returns ranked options,
        // best first, capped at maxOptions (worst kept separately for savings).
        Q_INVOKABLE QVariantList advise(int src_currency, int dst_currency, qint64 amount_minor) const;

        // Convenience: the single best option as a map (empty when none).
        Q_INVOKABLE QVariantMap best_route(int src_currency, int dst_currency, qint64 amount_minor) const;

        static constexpr int max_returned_options = 3;

    private:
        struct venue
        {
            bank_id id;
            bool cooling_down;
            QString cooldown_note;
        };

        QVector<venue> usable_venues() const;
        qint64 largest_funding_at(bank_id bank, currency_code ccy, int* account_id) const;
        bool has_account_in(bank_id bank, currency_code ccy) const;

        account_service& accounts_;
        connection_service& connections_;
        fx_service& fx_;
    };

} // namespace kmx
