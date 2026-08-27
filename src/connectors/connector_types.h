/// @file src/connectors/connector_types.h
/// @brief DTOs exchanged between connectors and the aggregation layer.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QString>
    #include <chrono>
    #include <optional>
#endif
#include "domain/account.h"
#include "domain/bank.h"
#include "domain/transaction.h"
namespace kmx::connector
{

    enum class sync_error_code : quint8
    {
        none = 0,
        invalid_credentials,
        session_expired,
        rate_limited,
        unavailable
    };

    struct sync_error
    {
        sync_error_code code {sync_error_code::none};
        QString detail;
        std::chrono::milliseconds retry_after {0};
    };

    inline sync_error make_error(sync_error_code code, QString detail = {}, std::chrono::milliseconds retry_after = {})
    {
        return sync_error {code, std::move(detail), retry_after};
    }

    /// @brief Credentials the mock login forms submit; connectors decide what to accept.
    struct mock_credentials
    {
        QString username;
        QString password;
        QString otp;
    };

    /// @brief Opaque-ish session handle returned by a successful authentication.
    struct remote_session
    {
        bank_id bank {bank_id::kmx_bank};
        QString token;
        QDateTime issued_at;
        std::chrono::minutes validity_minutes {60};

        bool is_expired(const QDateTime& at_now) const { return issued_at.addSecs(validity_minutes.count() * 60) < at_now; }
    };

    /// @brief Normalized DTOs.
    /// @details Connectors translate bank-specific payloads into these; everything downstream of bank_connector sees only this shape.
    struct remote_account
    {
        QString external_id;
        QString name;
        kmx::account_kind kind {kmx::account_kind::checking};
        kmx::currency_code currency {kmx::currency_code::ron};
        QString iban;
        qint64 balance_minor {0};
        qint64 pending_hold_minor {0};
    };

    struct remote_transaction
    {
        QString external_id;
        QString account_external_id;
        kmx::txn_direction direction {kmx::txn_direction::debit};
        QString counterparty_raw;
        bool has_counterparty_iban {false};
        QString counterparty_iban;
        qint64 amount_minor {0};
        kmx::currency_code amount_currency {kmx::currency_code::ron};
        QDateTime posted_at;
        kmx::txn_status status {kmx::txn_status::booked};
        // Category knowledge varies per bank (plan §2): native, absent (aggregator
        // infers from counterparty_raw later), or partial.
        kmx::category_origin category_source {kmx::category_origin::native};
        kmx::txn_category category {kmx::txn_category::other};
    };

} // namespace kmx::connector
