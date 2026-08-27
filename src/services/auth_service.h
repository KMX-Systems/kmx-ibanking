/// @file src/services/auth_service.h
/// @brief Simulated authentication, OTP and session lifecycle.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
    #include <QObject>
    #include <chrono>
    #include <functional>
#endif
#include "services/clock_source.h"
namespace kmx
{

    /// @brief Simulated authentication + session lifecycle (plan §Phase 2 / §4 timings):
    /// @details LoggedOut --authenticate()--> AwaitingOtp --> Active ^ | | | (bad/expired otp) |-- lock() --> Locked --unlock()-+ +----
    /// logout() <-------------+---------------+--------------------------------+ 5 consecutive failures (password or OTP) trigger a 30 s
    /// lockout during which authenticate()/verify_otp() are refused. OTP codes live 60 s. Latency theater is injectable: tests construct
    /// with zero delays.
    class auth_service: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int state READ state NOTIFY state_changed)
        Q_PROPERTY(bool busy READ busy NOTIFY busy_changed)
        Q_PROPERTY(QString display_name READ display_name NOTIFY state_changed)
        Q_PROPERTY(int attempts_left READ attempts_left NOTIFY failure_counters_changed)
        Q_PROPERTY(int idle_lock_seconds CONSTANT READ idle_lock_seconds)

    public:
        /// @brief NOTE (style exception):
        /// @details the enumerators below stay PascalCase even though the guide asks for lowercase constants. Qt only exposes Q_ENUM keys
        /// to QML when they begin with an uppercase letter -- QML reads `AuthService.Active`, and a lowercase key silently evaluates to
        /// `undefined` (verified, not assumed).
        enum class auth_state : quint8
        {
            LoggedOut = 0,
            AwaitingOtp,
            Active,
            Locked
        };
        Q_ENUM(auth_state)

        static constexpr int max_failures = 5;
        static constexpr int lockout_seconds = 30;
        static constexpr int otp_validity_seconds = 60;
        static constexpr int idle_lock_timeout_seconds = 300;

        explicit auth_service(clock_source& clock, QObject* parent = nullptr);

        // Production latency (ms); tests pass 0 to run synchronously.
        void set_latency(std::chrono::milliseconds auth_delay, std::chrono::milliseconds verify_delay);

        int state() const { return static_cast<int>(state_); }
        bool busy() const { return busy_; }
        QString display_name() const
        {
            return state_ == auth_state::Active || state_ == auth_state::Locked ? QStringLiteral("Ana Dumitrescu") : QString();
        }
        int attempts_left() const { return max_failures - fail_streak_; }
        int idle_lock_seconds() const { return idle_lock_timeout_seconds; }
        Q_INVOKABLE int lockout_seconds_left() const; // polled by QML each second

        Q_INVOKABLE void authenticate(const QString& username, const QString& password);
        Q_INVOKABLE void verify_otp(const QString& code);
        Q_INVOKABLE void resend_otp();
        Q_INVOKABLE void lock();
        Q_INVOKABLE bool unlock(const QString& password);
        Q_INVOKABLE void logout();

    signals:
        void state_changed(int new_state);
        void busy_changed();
        void failure_counters_changed();
        // Simulated SMS delivery — the UI shows it as an SMS preview card.
        void otp_issued(const QString& code, int validity_seconds);
        void login_failed(const QString& message);
        void login_succeeded();
        void locked();
        void unlocked();
        void logged_out();

    private:
        bool in_lockout() const;
        void begin_lockout();
        void set_state(auth_state next);
        void issue_otp();
        void reset_for_logout();
        void schedule(std::chrono::milliseconds delay, std::function<void()> fn);

        clock_source& clock_;
        std::chrono::milliseconds auth_delay_ {500};
        std::chrono::milliseconds verify_delay_ {300};

        auth_state state_ {auth_state::LoggedOut};
        bool busy_ {false};
        QString pending_username_;
        QDateTime lockout_until_;
        int fail_streak_ {0};
        QString otp_code_;
        QDateTime otp_expires_at_;
    };

} // namespace kmx
