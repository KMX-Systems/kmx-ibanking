/// @file src/services/notification_service.h
/// @brief Notification bus behind the bell badge, toasts and drawer.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QObject>
    #include <QStringList>
    #include <QVariantList>
#endif
namespace kmx
{

    /// @brief Minimal notification bus for Phase 3 (sync completions, re-auth alerts).
    /// @details The full center drawer with read/unread + severity styling lands later; this already powers the bell badge and toast
    /// deep-links.
    class notification_service: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int unread_count READ unread_count NOTIFY unread_count_changed)
        // NOTIFY-driven so the drawer re-reads when notifications arrive or are
        // marked read; a plain invokable would bind fire-once and go stale.
        Q_PROPERTY(QVariantList items_list READ items_list NOTIFY items_changed)
    public:
        explicit notification_service(QObject* parent = nullptr);

        struct item
        {
            int id {0};
            QString level; // info | success | warning | critical
            QString title;
            QString body;
            QString deep_link_key; // shell route key, empty if none
            QDateTime at;
            bool read {false};
        };

        Q_INVOKABLE void post(const QString& level, const QString& title, const QString& body = {}, const QString& deep_link_key = {});
        Q_INVOKABLE void mark_all_read();
        Q_INVOKABLE QVariantList items_list() const; // newest first, for the drawer

        int unread_count() const { return unread_; }
        QVector<item> items() const { return items_; }

    signals:
        void posted(int id, const QString& level, const QString& title, const QString& body, const QString& deep_link_key);
        void items_changed();
        void unread_count_changed();

    private:
        QVector<item> items_;
        int unread_ {0};
        int next_id_ {1};
    };

} // namespace kmx
