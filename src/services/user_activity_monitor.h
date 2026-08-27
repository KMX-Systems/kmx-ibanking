/// @file src/services/user_activity_monitor.h
/// @brief Passive input observer powering idle auto-lock.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QObject>
#endif
namespace kmx
{

    /// @brief Tracks genuine user input at application level so the shell can implement idle auto-lock (plan §4:
    /// @details 5 min default). Passive observer: never consumes events. QML polls `seconds_idle` from a 1 Hz timer.
    class user_activity_monitor final: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int seconds_idle READ seconds_idle NOTIFY activity_occurred)
    public:
        explicit user_activity_monitor(QObject* parent = nullptr);

        void install();

        int seconds_idle() const;
        QDateTime last_activity() const { return last_activity_; }

    signals:
        void activity_occurred();

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:
        QDateTime last_activity_;
    };

} // namespace kmx
