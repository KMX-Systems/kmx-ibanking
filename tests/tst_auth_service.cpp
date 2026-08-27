/// @file tests/tst_auth_service.cpp
/// @brief Authentication, OTP, lockout and lock/unlock tests.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "services/auth_service.h"
#include "services/clock_source.h"
#include <QSignalSpy>
#include <QtTest>

using namespace kmx;
using auth_state = auth_service::auth_state;

class tst_AuthService final: public QObject
{
    Q_OBJECT

private slots:
    void happy_path_issues_otp_then_activates();
    void empty_fields_do_not_consume_attempts();
    void wrong_codes_count_toward_lockout();
    void lockout_blocks_until_expiry();
    void otp_expiry_forces_resend();
    void lock_unlock_cycle();
    void logout_resets_everything();

private:
    static QSharedPointer<fake_clock> clock() { return QSharedPointer<fake_clock>::create(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0))); }
};

void tst_AuthService::happy_path_issues_otp_then_activates()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});

    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);
    QSignalSpy ok_spy(&auth, &auth_service::login_succeeded);

    QCOMPARE(auth.state(), int(auth_state::LoggedOut));

    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));
    QCOMPARE(auth.state(), int(auth_state::AwaitingOtp));
    QCOMPARE(otp_spy.count(), 1);

    const QString code = otp_spy.first().first().toString();
    QCOMPARE(code.size(), 6);

    auth.verify_otp(code);
    QCOMPARE(ok_spy.count(), 1);
    QCOMPARE(auth.state(), int(auth_state::Active));
    QCOMPARE(auth.display_name(), QStringLiteral("Ana Dumitrescu"));
}

void tst_AuthService::empty_fields_do_not_consume_attempts()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});

    QSignalSpy fail_spy(&auth, &auth_service::login_failed);

    auth.authenticate(QString(), QStringLiteral("x"));
    auth.authenticate(QStringLiteral("user"), QString());

    QCOMPARE(fail_spy.count(), 2);
    QCOMPARE(auth.state(), int(auth_state::LoggedOut));
    QCOMPARE(auth.attempts_left(), auth_service::max_failures); // untouched
}

void tst_AuthService::wrong_codes_count_toward_lockout()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});
    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);

    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));
    QCOMPARE(auth.attempts_left(), auth_service::max_failures);

    // Guaranteed-wrong code: flip the first digit of the issued one.
    QString wrong = otp_spy.last().first().toString();
    wrong[0] = wrong.at(0) == u'0' ? u'1' : u'0';

    for (int i = 0; i < auth_service::max_failures - 1; ++i)
    {
        auth.verify_otp(wrong);
        QCOMPARE(auth.attempts_left(), auth_service::max_failures - 1 - i);
    }
}

void tst_AuthService::lockout_blocks_until_expiry()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});
    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);

    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));

    QString wrong = otp_spy.last().first().toString();
    wrong[0] = wrong.at(0) == u'0' ? u'1' : u'0';
    for (int i = 0; i < auth_service::max_failures; ++i)
        auth.verify_otp(wrong);

    QCOMPARE(auth.state(), int(auth_state::LoggedOut));
    QVERIFY(auth.lockout_seconds_left() > 0);

    // Blocked while cooling down.
    QSignalSpy fail_spy(&auth, &auth_service::login_failed);
    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));
    QCOMPARE(fail_spy.count(), 1);
    QCOMPARE(auth.state(), int(auth_state::LoggedOut));

    // Time travel past the cooldown re-enables login.
    c->advance_secs(auth_service::lockout_seconds + 1);
    QVERIFY(auth.lockout_seconds_left() == 0);
    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));
    QCOMPARE(auth.state(), int(auth_state::AwaitingOtp));
}

void tst_AuthService::otp_expiry_forces_resend()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});
    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);

    auth.authenticate(QStringLiteral("ana.demo"), QStringLiteral("secret"));
    const QString first_code = otp_spy.last().first().toString();

    c->advance_secs(auth_service::otp_validity_seconds + 1);
    QSignalSpy fail_spy(&auth, &auth_service::login_failed);
    auth.verify_otp(first_code); // valid code, but expired
    QCOMPARE(fail_spy.count(), 1);
    QCOMPARE(auth.state(), int(auth_state::AwaitingOtp)); // stays for retry/resend

    auth.resend_otp();
    QCOMPARE(otp_spy.count(), 2);
    const QString fresh = otp_spy.last().first().toString();
    auth.verify_otp(fresh);
    QCOMPARE(auth.state(), int(auth_state::Active));
}

void tst_AuthService::lock_unlock_cycle()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});
    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);

    auth.authenticate(QStringLiteral("u"), QStringLiteral("p"));
    auth.verify_otp(otp_spy.last().first().toString());
    QCOMPARE(auth.state(), int(auth_state::Active));

    QSignalSpy locked_spy(&auth, &auth_service::locked);
    auth.lock();
    QCOMPARE(locked_spy.count(), 1);
    QCOMPARE(auth.state(), int(auth_state::Locked));

    QVERIFY(!auth.unlock(QString()));            // empty refused
    QVERIFY(auth.unlock(QStringLiteral("any"))); // password-only unlock
    QCOMPARE(auth.state(), int(auth_state::Active));
}

void tst_AuthService::logout_resets_everything()
{
    auto c = clock();
    auth_service auth(*c);
    auth.set_latency({}, {});
    QSignalSpy out_spy(&auth, &auth_service::logged_out);

    auth.authenticate(QStringLiteral("u"), QStringLiteral("p"));
    auth.logout();
    QCOMPARE(out_spy.count(), 1);
    QCOMPARE(auth.state(), int(auth_state::LoggedOut));
    QCOMPARE(auth.attempts_left(), auth_service::max_failures);

    // Fresh full flow works afterwards.
    QSignalSpy otp_spy(&auth, &auth_service::otp_issued);
    auth.authenticate(QStringLiteral("u"), QStringLiteral("p"));
    auth.verify_otp(otp_spy.last().first().toString());
    QCOMPARE(auth.state(), int(auth_state::Active));
}

QTEST_MAIN(tst_AuthService)
#include "tst_auth_service.moc"
