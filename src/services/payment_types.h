/// @file src/services/payment_types.h
/// @brief Transfer requests, receipts and the payment error catalog.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QString>
#endif
#include "domain/transaction.h"
namespace kmx
{

    /// @brief Error codes mirror the plan §4 catalog subset relevant to payments.
    enum class payment_error_code : quint8
    {
        none = 0,
        validation_required, // generic form-level problem
        insufficient_funds,
        amount_too_small,
        amount_too_large,
        invalid_iban,
        same_source_and_destination,
        currency_pair_unsupported, // cross-currency waits for the P7 advisor
        capability_missing         // e.g. scheduled payments at a bank that lacks them
    };

    struct payment_error
    {
        payment_error_code code {payment_error_code::none};
        QString message; // already translated, user-presentable
    };

    struct transfer_request
    {
        qint64 source_account_id {-1};
        QString beneficiary_iban;
        QString beneficiary_name;
        qint64 amount_minor {0};
        QString note;
    };

    struct validated_transfer
    {
        transfer_request request;
        qint64 destination_account_id {-1}; // >= 0 when internal
        currency_code source_currency {currency_code::ron};
    };

    struct transfer_receipt
    {
        QString reference;
        QDateTime booked_at;
        QString source_name;
        QString beneficiary_name;
        qint64 amount_minor {0};
        currency_code currency {currency_code::ron};
        bool internal_ {false};
    };

    struct scheduled_payment
    {
        qint64 id {0};
        qint64 source_account_id {-1};
        QString beneficiary_iban;
        QString beneficiary_name;
        qint64 amount_minor {0};
        int day_of_month {1};
        QDate next_run;
        QString note;
    };

} // namespace kmx
