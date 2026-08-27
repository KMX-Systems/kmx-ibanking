/// @file src/services/user_activity_monitor.cpp
/// @brief Application-level event filter tracking genuine user input.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "user_activity_monitor.h"
#include <QCoreApplication>
#include <QEvent>

namespace kmx
{

    namespace detail
    {
        bool is_user_input(QEvent::Type t)
        {
            switch (t)
            {
                case QEvent::MouseMove:
                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonRelease:
                case QEvent::Wheel:
                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                case QEvent::TabletPress:
                case QEvent::TouchBegin:
                case QEvent::TouchUpdate:
                    return true;
                default:
                    return false;
            }
        }
    } // namespace detail
    user_activity_monitor::user_activity_monitor(QObject* parent): QObject(parent), last_activity_(QDateTime::currentDateTime())
    {
    }

    void user_activity_monitor::install()
    {
        if (QCoreApplication* app = QCoreApplication::instance())
            app->installEventFilter(this);
    }

    int user_activity_monitor::seconds_idle() const
    {
        return static_cast<int>(last_activity_.secsTo(QDateTime::currentDateTime()));
    }

    bool user_activity_monitor::eventFilter(QObject* obj, QEvent* event)
    {
        if (detail::is_user_input(event->type()))
        {
            last_activity_ = QDateTime::currentDateTime();
            emit activity_occurred();
        }
        return QObject::eventFilter(obj, event);
    }

} // namespace kmx
