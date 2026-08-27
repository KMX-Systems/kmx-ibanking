/// @file src/domain/money.h
/// @brief Exact money type: minor units plus currency, never floating point.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#ifndef PCH
    #include <QMetaType>
#endif
#include "currency.h"
namespace kmx
{

    /// @brief money is qint64 minor units + currency.
    /// @details Doubles are banned from money math. Rates are expressed as qint64 micro-units: how many target MAJOR units one SOURCE major
    /// unit buys, scaled by 1e6. Example: EUR->RON mid 4.972300 becomes rate_micro = 4'972'300. This keeps every product inside qint64 for
    /// any realistic amount (< 1e10 minor) and makes rounding explicit and tested.
    class money
    {
    public:
        constexpr money() noexcept = default;
        constexpr money(qint64 minor_units, currency_code c) noexcept: minor_(minor_units), currency_(c) {}

        constexpr qint64 minor() const noexcept { return minor_; }
        constexpr currency_code currency() const noexcept { return currency_; }

        constexpr bool is_same_currency(const money& o) const noexcept { return currency_ == o.currency_; }

        constexpr money operator+(const money& o) const noexcept
        {
            Q_ASSERT(is_same_currency(o));
            return money(minor_ + o.minor_, currency_);
        }

        constexpr money operator-(const money& o) const noexcept
        {
            Q_ASSERT(is_same_currency(o));
            return money(minor_ - o.minor_, currency_);
        }

        constexpr money& operator+=(const money& o) noexcept
        {
            Q_ASSERT(is_same_currency(o));
            minor_ += o.minor_;
            return *this;
        }

        constexpr money operator-() const noexcept { return money(-minor_, currency_); }

        constexpr bool operator==(const money& o) const noexcept { return minor_ == o.minor_ && currency_ == o.currency_; }

        constexpr bool operator<(const money& o) const noexcept
        {
            Q_ASSERT(is_same_currency(o));
            return minor_ < o.minor_;
        }

        // Exact constructor from major units; never goes through floating point.
        static constexpr money from_major(qint64 major, currency_code c) noexcept { return money(major * minor_factor, c); }

        // Converts this amount into `target` using rate_micro (see class comment),
        // rounding half away from zero to the target minor unit.
        static money convert(const money& input, currency_code target, qint64 rate_micro) noexcept
        {
            return money(div_round_half_away(input.minor_ * rate_micro, rate_denominator), target);
        }

        // Reciprocal rate at micro precision (sufficient for demo pricing;
        // the exchange advisor always recomputes legs exactly, never chains inverses).
        static constexpr qint64 inverse_rate_micro(qint64 rate_micro) noexcept
        {
            return (inverse_denominator + rate_micro / 2) / rate_micro;
        }

        static constexpr qint64 rate_denominator = 1'000'000LL;

    private:
        static constexpr qint64 inverse_denominator = 1'000'000'000'000LL;

        static constexpr qint64 div_round_half_away(qint64 num, qint64 den) noexcept
        {
            const qint64 sign = num < 0 ? -1 : 1;
            const qint64 n = num * sign;
            return sign * ((n * 2 + den) / (den * 2));
        }

        qint64 minor_ {0};
        currency_code currency_ {currency_code::ron};
    };

} // namespace kmx

Q_DECLARE_METATYPE(kmx::money)
