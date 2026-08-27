/// @file src/domain/transaction.h
/// @brief One ledger entry, normalized across every linked bank.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QMetaType>
    #include <QString>
    #include <QtGlobal>
#endif
#include "account.h"
#include "currency.h"
namespace kmx
{

    enum class txn_direction : quint8
    {
        credit = 0,
        debit = 1
    };

    enum class txn_category : quint8
    {
        salary = 0,
        groceries,
        dining,
        transport,
        utilities,
        shopping,
        health,
        entertainment,
        travel,
        fees,
        transfer,
        interest,
        fx,
        other
    };

    inline constexpr int txn_category_count = 14;

    /// @brief Where the category came from:
    /// @details the bank natively, inferred by our aggregator from raw merchant strings, or set manually by the user.
    enum class category_origin : quint8
    {
        native = 0,
        inferred = 1,
        manual = 2
    };

    enum class txn_status : quint8
    {
        pending = 0,
        booked = 1
    };

    using transaction_id_t = qint64;

    struct transaction
    {
        transaction_id_t id {0};
        account_id_t account_id {0};
        txn_direction direction {txn_direction::debit};
        QString counterparty;
        QString counterparty_iban; // empty when unknown
        txn_category category {txn_category::other};
        category_origin category_source {category_origin::native};
        qint64 amount_minor {0};                     // magnitude; sign lives in direction
        currency_code currency {currency_code::ron}; // booking currency of the account
        QString fx_note;                             // e.g. "1 EUR = 4.9723 RON" for converted bookings
        QDateTime posted_at;
        txn_status status {txn_status::booked};
        QString reference;
        QString note;

        qint64 signed_amount_minor() const { return direction == txn_direction::credit ? amount_minor : -amount_minor; }
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::transaction)
