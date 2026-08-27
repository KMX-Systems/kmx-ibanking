/// @file src/domain/iban.cpp
/// @brief IBAN checksum, normalization and masking implementation.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "iban.h"
#include <QChar>
#include <algorithm>

namespace kmx
{

    namespace detail
    {

        // Standard ISO 7064 MOD-97-10 over the rearranged representation.
        int mod97(const QString& digits_and_mapped)
        {
            int remainder = 0;
            for (const QChar ch: digits_and_mapped)
            {
                if (ch.isDigit())
                {
                    remainder = (remainder * 10 + (ch.unicode() - u'0')) % 97;
                }
                else if (ch.isLetter())
                {
                    const int value = 10 + (ch.toUpper().unicode() - u'A');
                    remainder = (remainder * 10 + value / 10) % 97;
                    remainder = (remainder * 10 + value % 10) % 97;
                }
                else
                {
                    return -1; // illegal character
                }
            }
            return remainder;
        }

    } // namespace detail
    bool is_valid_iban(const QString& input)
    {
        QString iban = input;
        iban.remove(u' ');
        iban.remove(u'-');
        if (iban.size() < 15 || iban.size() > 34)
            return false;
        if (!iban.at(0).isLetter() || !iban.at(1).isLetter())
            return false;

        const QString rearranged = iban.mid(4) + iban.left(4);
        return detail::mod97(rearranged) == 1;
    }

    QString make_romanian_iban(QStringView bank_code4, qint64 serial)
    {
        const QString bban = QStringLiteral("%1%2").arg(bank_code4.toString()).arg(serial, 12, 10, QLatin1Char('0'));
        const QString candidate = bban + QStringLiteral("RO00");
        const int remainder = detail::mod97(candidate);
        const int check = 98 - remainder;

        return QStringLiteral("RO%1%2").arg(check, 2, 10, QLatin1Char('0')).arg(bban);
    }

} // namespace kmx
