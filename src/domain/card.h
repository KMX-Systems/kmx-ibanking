/// @file src/domain/card.h
/// @brief Payment card attached to an account: PAN, limits and toggles.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaType>
    #include <QString>
    #include <QtGlobal>
#endif
namespace kmx
{

    using card_id_t = qint64;

    enum class card_network : quint8
    {
        visa = 0,
        mastercard = 1
    };

    struct card
    {
        card_id_t id {0};
        qint64 account_id {0};
        QString label;
        card_network network {card_network::visa};
        QString full_pan;   // revealed only via hold-to-show
        QString cvv;        // ditto
        QString masked_pan; // e.g. "4539 •••• •••• 1234" (derived)
        QString expiry_mmyy;
        QString holder_name;
        bool frozen {false};
        bool online_payments {true};
        bool contactless {true};
        bool is_virtual {false};
        qint64 daily_limit_minor {0}; // in account currency
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::card)
