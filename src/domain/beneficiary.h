/// @file src/domain/beneficiary.h
/// @brief Saved transfer counterparty: name, IBAN and favourite flag.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QMetaType>
    #include <QString>
    #include <QtGlobal>
#endif
#include "currency.h"
namespace kmx
{

    using beneficiary_id_t = qint64;

    struct beneficiary
    {
        beneficiary_id_t id {0};
        QString name;
        QString iban;
        currency_code default_currency {currency_code::ron};
        bool favorite {false};
        QDateTime last_used_at; // null when never used
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::beneficiary)
