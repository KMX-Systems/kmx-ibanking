/// @file src/services/auth_service.cpp
/// @brief Auth state machine, OTP issuance and lockout handling.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "auth_service.h"
#include <QRandomGenerator>
#include <QTimer>

namespace kmx
{

    auth_service::auth_service(clock_source& clock, QObject* parent): QObject(parent), clock_(clock)
    {
    }

    void auth_service::set_latency(std::chrono::milliseconds auth_delay, std::chrono::milliseconds verify_delay)
    {
        auth_delay_ = auth_delay;
        verify_delay_ = verify_delay;
    }

    // Zero delay => synchronous execution so unit tests need no event loop.
    void auth_service::schedule(std::chrono::milliseconds delay, std::function<void()> fn)
    {
        if (delay.count() <= 0)
            fn();
        else
            QTimer::singleShot(delay, this, std::move(fn));
    }

    int auth_service::lockout_seconds_left() const
    {
        if (!in_lockout())
            return 0;
        return static_cast<int>(clock_.now().secsTo(lockout_until_));
    }

    bool auth_service::in_lockout() const
    {
        return fail_streak_ >= max_failures && clock_.now() < lockout_until_;
    }

    void auth_service::begin_lockout()
    {
        lockout_until_ = clock_.now().addSecs(lockout_seconds);
    }

    void auth_service::set_state(auth_state next)
    {
        if (state_ == next)
            return;
        state_ = next;
        emit state_changed(static_cast<int>(next));
    }

    void auth_service::authenticate(const QString& username, const QString& password)
    {
        // Already signed in: ignore re-entrant logins instead of demoting the
        // session back to AwaitingOtp.
        if (busy_ || state_ == auth_state::Locked || state_ == auth_state::Active)
            return;

        if (in_lockout())
        {
            emit login_failed(tr("Too many attempts. Try again in %1 s.").arg(lockout_seconds_left()));
            return;
        }

        // Cosmetic client-side check; does NOT consume an attempt.
        if (username.trimmed().isEmpty() || password.isEmpty())
        {
            emit login_failed(tr("Enter your username and password."));
            return;
        }

        busy_ = true;
        emit busy_changed();
        pending_username_ = username;
        set_state(auth_state::AwaitingOtp);

        schedule(auth_delay_,
                 [this]()
                 {
                     busy_ = false;
                     emit busy_changed();
                     issue_otp();
                 });
    }

    void auth_service::issue_otp()
    {
        const quint32 raw = QRandomGenerator::global()->bounded(1'000'000u);
        otp_code_ = QStringLiteral("%1").arg(raw, 6, 10, QLatin1Char('0'));
        otp_expires_at_ = clock_.now().addSecs(otp_validity_seconds);
        emit otp_issued(otp_code_, otp_validity_seconds);
    }

    void auth_service::verify_otp(const QString& code)
    {
        if (busy_ || state_ != auth_state::AwaitingOtp)
            return;

        if (in_lockout())
        {
            emit login_failed(tr("Too many attempts. Try again in %1 s.").arg(lockout_seconds_left()));
            return;
        }

        if (clock_.now() > otp_expires_at_)
        {
            emit login_failed(tr("Code expired. Request a new one."));
            return;
        }

        if (code.compare(otp_code_) != 0)
        {
            ++fail_streak_;
            emit failure_counters_changed();
            if (fail_streak_ >= max_failures)
            {
                // Keep the lockout timestamp + counters intact; the login screen
                // reads lockout_seconds_left() and re-enables after the cooldown.
                begin_lockout();
                // NOTE: no logged_out() here — that signal makes the session drop
                // every bank link, and a failed OTP streak must not do that.
                set_state(auth_state::LoggedOut);
                emit login_failed(tr("Too many attempts. Locked for %1 s.").arg(lockout_seconds));
            }
            else
            {
                emit login_failed(tr("Incorrect code. Attempts left: %1.").arg(attempts_left()));
            }
            return;
        }

        busy_ = true;
        emit busy_changed();
        schedule(verify_delay_,
                 [this]()
                 {
                     busy_ = false;
                     emit busy_changed();
                     fail_streak_ = 0;
                     otp_code_.clear();
                     pending_username_.clear();
                     emit failure_counters_changed();
                     set_state(auth_state::Active);
                     emit login_succeeded();
                 });
    }

    void auth_service::resend_otp()
    {
        if (state_ == auth_state::AwaitingOtp && !in_lockout())
            issue_otp();
    }

    void auth_service::lock()
    {
        if (state_ == auth_state::Active)
        {
            set_state(auth_state::Locked);
            emit locked();
        }
    }

    bool auth_service::unlock(const QString& password)
    {
        if (state_ != auth_state::Locked || password.isEmpty())
            return false;
        set_state(auth_state::Active);
        emit unlocked();
        return true;
    }

    void auth_service::logout()
    {
        if (state_ == auth_state::LoggedOut && !in_lockout())
            return;

        if (in_lockout())
        {
            // Logging out during a cooldown must not reset it — otherwise
            // "Back to login" becomes a lockout bypass.
            pending_username_.clear();
            otp_code_.clear();
        }
        else
        {
            reset_for_logout();
        }
        set_state(auth_state::LoggedOut);
        emit logged_out();
    }

    void auth_service::reset_for_logout()
    {
        pending_username_.clear();
        otp_code_.clear();
        fail_streak_ = 0;
        emit failure_counters_changed();
    }

} // namespace kmx
