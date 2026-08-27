/// @file src/services/clock_source.h
/// @brief Injectable clock so time-dependent behaviour stays testable.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QDateTime>
#endif
/// @brief Injectable clock:
/// @details production uses system_clock; tests and the seeded world generator use fake_clock so behavior is deterministic.
class clock_source
{
public:
    virtual ~clock_source() = default;
    virtual QDateTime now() const = 0;
};

class system_clock final: public clock_source
{
public:
    QDateTime now() const override { return QDateTime::currentDateTime(); }
};

class fake_clock final: public clock_source
{
public:
    explicit fake_clock(QDateTime start): now_(std::move(start)) {}

    QDateTime now() const override { return now_; }
    void set_now(const QDateTime& t) { now_ = t; }
    void advance_secs(qint64 s) { now_ = now_.addSecs(s); }
    void advance_msecs(qint64 ms) { now_ = QDateTime::fromMSecsSinceEpoch(now_.toMSecsSinceEpoch() + ms); }

private:
    QDateTime now_;
};
