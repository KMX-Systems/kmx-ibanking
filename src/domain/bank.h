/// @file src/domain/bank.h
/// @brief Bank roster: identifiers, display names, brand colours and logos.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaType>
    #include <QString>
    #include <QtGlobal>
#endif
namespace kmx
{

    enum class bank_id : quint8
    {
        kmx_bank = 0,
        banca_transilvania = 1,
        tbi_bank = 2,
        erste_bank = 3
    };

    inline constexpr int bank_count = 4;

    inline const char* bank_name(bank_id id) noexcept
    {
        switch (id)
        {
            case bank_id::kmx_bank:
                return "KMX Bank";
            case bank_id::banca_transilvania:
                return "Banca Transilvania";
            case bank_id::tbi_bank:
                return "TBI bank";
            case bank_id::erste_bank:
                return "Erste Bank";
        }
        return "?";
    }

    // Brand color as 0xRRGGBB. Deliberately a plain integer: kmxcore stays
    // QtCore-only; QML converts to QColor at the presentation boundary.
    inline quint32 bank_brand_color_rgb(bank_id id) noexcept
    {
        switch (id)
        {
            case bank_id::kmx_bank:
                return 0x16324f;
            case bank_id::banca_transilvania:
                return 0xEC2127;
            case bank_id::tbi_bank:
                return 0xFF6600;
            case bank_id::erste_bank:
                return 0x2870ED;
        }
        return 0x808080;
    }

    // Empty string for KMX until the monogram is designed (Phase 1).
    inline QString bank_logo_source(bank_id id)
    {
        switch (id)
        {
            case bank_id::banca_transilvania:
                return QStringLiteral("qrc:/kmx/logos/bt-logo.svg");
            case bank_id::tbi_bank:
                return QStringLiteral("qrc:/kmx/logos/tbi-logo.svg");
            case bank_id::erste_bank:
                return QStringLiteral("qrc:/kmx/logos/erste-logo.svg");
            case bank_id::kmx_bank:
                break;
        }
        return {};
    }

    struct bank_info
    {
        bank_id id {bank_id::kmx_bank};
        QString name;
        quint32 brand_color_rgb {0x808080};
        QString logo_source;
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::bank_id)
