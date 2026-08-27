/// @file src/domain/iban.h
/// @brief IBAN validation and formatting (ISO 7064 MOD-97-10).
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QString>
    #include <QStringView>
#endif
namespace kmx
{

    // ISO 7064 MOD-97-10 validation; accepts spaces/dashes, case-insensitive.
    bool is_valid_iban(const QString& input);

    // Builds a checksum-valid Romanian IBAN (RO + 2 check digits + 4-letter bank
    // code + 12-digit serial). Used by the seeder now and the payment wizard later.
    QString make_romanian_iban(QStringView bank_code4, qint64 serial);

} // namespace kmx
