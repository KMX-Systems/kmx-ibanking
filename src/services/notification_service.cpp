/// @file src/services/notification_service.cpp
/// @brief Notification storage, read state and dispatch.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "notification_service.h"

namespace kmx
{

    notification_service::notification_service(QObject* parent): QObject(parent)
    {
    }

    void notification_service::post(const QString& level, const QString& title, const QString& body, const QString& deep_link_key)
    {
        item item;
        item.id = next_id_++;
        item.level = level;
        item.title = title;
        item.body = body;
        item.deep_link_key = deep_link_key;
        item.at = QDateTime::currentDateTime();

        items_.prepend(item); // newest first
        if (items_.size() > 50)
            items_.removeLast();
        ++unread_;
        emit items_changed();
        emit unread_count_changed();
        emit posted(item.id, item.level, item.title, item.body, item.deep_link_key);
    }

    void notification_service::mark_all_read()
    {
        if (unread_ == 0)
            return;
        for (auto& i: items_)
            i.read = true;
        unread_ = 0;
        emit items_changed();
        emit unread_count_changed();
    }

    QVariantList notification_service::items_list() const
    {
        QVariantList out;
        for (const auto& i: items_)
        {
            QVariantMap row;
            row["id"] = i.id;
            row["level"] = i.level;
            row["title"] = i.title;
            row["body"] = i.body;
            row["deep_link_key"] = i.deep_link_key;
            row["at"] = i.at;
            row["read"] = i.read;
            out.append(row);
        }
        return out;
    }

} // namespace kmx
