/// @file src/domain/currency.h
/// @brief Supported currencies and their ISO codes.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaType>
    #include <QtGlobal>
#endif
namespace kmx
{

    enum class currency_code : quint8
    {
        ron = 0,
        eur = 1,
        usd = 2
    };

    inline constexpr int currency_count = 3;

    inline constexpr const char* to_code(currency_code c) noexcept
    {
        switch (c)
        {
            case currency_code::ron:
                return "RON";
            case currency_code::eur:
                return "EUR";
            case currency_code::usd:
                return "USD";
        }
        return "???";
    }

    // All supported currencies use two minor digits (bani / cents).
    inline constexpr qint64 minor_factor = 100;

} // namespace kmx

Q_DECLARE_METATYPE(kmx::currency_code)
