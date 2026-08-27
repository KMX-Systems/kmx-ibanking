/// @file tests/tst_connection_service.cpp
/// @brief Link lifecycle, sync failure and re-auth tests.
/// Phase 3).
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "connectors/kmx_connector.h"
#include "domain/seed_world.h"
#include "services/clock_source.h"
#include "services/connection_service.h"
#include <QtTest>

using namespace kmx;
using link_state = connection_service::link_state;

// Configurable test double exercising every state-machine branch without
// pulling in the real BT/Erste connectors (those arrive in Phase 3).
class stub_connector final: public bank_connector
{
public:
    stub_connector(clock_source& clock, bank_id bank, std::chrono::minutes session_validity, bool reject_auth = false):
        clock_(clock),
        bank_(bank),
        validity_(session_validity),
        reject_auth_(reject_auth)
    {
    }

    bank_id bank() const override { return bank_; }
    bank_capabilities capabilities() const override { return {}; }
    fx_desk desk() const override { return fx_desk(bank_); }

    int auth_calls = 0;

    std::expected<connector::remote_session, connector::sync_error> authenticate(const connector::mock_credentials&) override
    {
        ++auth_calls;
        if (reject_auth_)
            return std::unexpected(connector::make_error(connector::sync_error_code::invalid_credentials, "nope"));
        connector::remote_session s;
        s.bank = bank_;
        s.token = "stub";
        s.issued_at = clock_.now();
        s.validity_minutes = validity_;
        return s;
    }

    std::expected<QVector<connector::remote_account>, connector::sync_error> fetch_accounts(const connector::remote_session&) override
    {
        if (fail_fetches_ > 0)
        {
            --fail_fetches_;
            return std::unexpected(connector::make_error(connector::sync_error_code::unavailable, "fetch failed"));
        }
        return QVector<connector::remote_account> {};
    }

    std::expected<QVector<connector::remote_transaction>, connector::sync_error> fetch_transactions(const connector::remote_session&,
                                                                                                    const QString&,
                                                                                                    std::chrono::seconds) override
    {
        if (rate_limit_once_)
        {
            rate_limit_once_ = false;
            return std::unexpected(
                connector::make_error(connector::sync_error_code::rate_limited, "cool down", std::chrono::milliseconds(300'000)));
        }
        return QVector<connector::remote_transaction> {};
    }

    void fail_next_fetch() { ++fail_fetches_; }
    void rate_limit_next_refresh() { rate_limit_once_ = true; }

private:
    clock_source& clock_;
    bank_id bank_;
    std::chrono::minutes validity_;
    bool reject_auth_ {false};
    int fail_fetches_ {0};
    bool rate_limit_once_ {false};
};

class tst_ConnectionService final: public QObject
{
    Q_OBJECT

private slots:
    void happy_path_connects_and_stamps_sync();
    void bad_credentials_return_to_disconnected();
    void expired_session_demands_reauth();
    void sync_failure_marks_error_then_recovers();
    void disconnect_resets_everything();
};

void tst_ConnectionService::happy_path_connects_and_stamps_sync()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0)));
    connection_service svc(clock);
    svc.register_connector(std::make_unique<stub_connector>(clock, bank_id::kmx_bank, std::chrono::minutes(60)));

    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::kmx_bank)))),
             int(link_state::Disconnected));

    const auto result = svc.connect_bank(bank_id::kmx_bank, {"user", "pass", ""});
    QVERIFY(result.has_value());
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::kmx_bank)))), int(link_state::Connected));
    QVERIFY(svc.last_sync_at(static_cast<int>(bank_id::kmx_bank)).isValid());
}

void tst_ConnectionService::bad_credentials_return_to_disconnected()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0)));
    connection_service svc(clock);
    svc.register_connector(std::make_unique<stub_connector>(clock, bank_id::kmx_bank, std::chrono::minutes(60),
                                                            /*reject_auth=*/true));

    const auto result = svc.connect_bank(bank_id::kmx_bank, {"user", "", ""});
    QVERIFY(!result.has_value());
    QCOMPARE(result.error().code, connector::sync_error_code::invalid_credentials);
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::kmx_bank)))),
             int(link_state::Disconnected));
    QVERIFY(!svc.last_sync_at(static_cast<int>(bank_id::kmx_bank)).isValid());
}

void tst_ConnectionService::expired_session_demands_reauth()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0)));
    connection_service svc(clock);
    // Session valid for a single minute: advancing the clock expires it.
    svc.register_connector(std::make_unique<stub_connector>(clock, bank_id::banca_transilvania, std::chrono::minutes(1)));

    QVERIFY(svc.connect_bank(bank_id::banca_transilvania, {"u", "p", ""}).has_value());
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::banca_transilvania)))),
             int(link_state::Connected));

    clock.advance_secs(3601);

    // Silent refresh must NOT re-authenticate: open-banking links require
    // an explicit user reconnect.
    QVERIFY(!svc.refresh(bank_id::banca_transilvania));
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::banca_transilvania)))),
             int(link_state::NeedsReauth));
}

void tst_ConnectionService::sync_failure_marks_error_then_recovers()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0)));
    connection_service svc(clock);
    auto stub = std::make_unique<stub_connector>(clock, bank_id::tbi_bank, std::chrono::minutes(600));
    stub_connector* raw = stub.get();
    svc.register_connector(std::move(stub));

    raw->fail_next_fetch();
    QVERIFY(svc.connect_bank(bank_id::tbi_bank, {"u", "p", ""}).has_value());
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::tbi_bank)))), int(link_state::SyncFailed));

    QVERIFY(svc.refresh(bank_id::tbi_bank));
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::tbi_bank)))), int(link_state::Connected));
}

void tst_ConnectionService::disconnect_resets_everything()
{
    fake_clock clock(QDateTime(QDate(2026, 8, 25), QTime(12, 0, 0)));
    connection_service svc(clock);
    svc.register_connector(std::make_unique<stub_connector>(clock, bank_id::kmx_bank, std::chrono::minutes(60)));

    QVERIFY(svc.connect_bank(bank_id::kmx_bank, {"u", "p", ""}).has_value());

    svc.disconnect(bank_id::kmx_bank);
    QCOMPARE(int(static_cast<connection_service::link_state>(svc.state(static_cast<int>(bank_id::kmx_bank)))),
             int(link_state::Disconnected));
    QVERIFY(!svc.last_sync_at(static_cast<int>(bank_id::kmx_bank)).isValid());
    QVERIFY(!svc.refresh(bank_id::kmx_bank));
}

QTEST_MAIN(tst_ConnectionService)
#include "tst_connection_service.moc"
