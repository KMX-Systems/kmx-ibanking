/// @file src/domain/account.h
/// @brief One account at one bank: balance, holds and spendable funds.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QMetaType>
    #include <QString>
    #include <QtGlobal>
#endif
#include "bank.h"
#include "currency.h"
namespace kmx
{

    enum class account_kind : quint8
    {
        checking = 0,
        savings = 1,
        credit = 2
    };

    using account_id_t = qint64;

    struct account
    {
        account_id_t id {0};
        bank_id bank {bank_id::kmx_bank};
        QString name;
        account_kind kind {account_kind::checking};
        currency_code currency {currency_code::ron};
        QString iban;
        qint64 balance_minor {0};      // credit accounts carry negative balances (outstanding debt)
        qint64 pending_hold_minor {0}; // reserved amounts not yet booked; reduce what's spendable
        QDateTime opened_at;

        // Holds always project onto the balance in the "worse" direction:
        // deposits lose spendable funds; credit lines grow their projected debt.
        qint64 available_minor() const { return balance_minor - pending_hold_minor; }
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::account)
