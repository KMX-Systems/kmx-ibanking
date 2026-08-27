/// @file src/services/exchange_advisor_service.cpp
/// @brief Route enumeration, cost model and deterministic ranking.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "exchange_advisor_service.h"
#include "domain/fx_desk.h"
#include "domain/money.h"
#include "services/account_service.h"
#include "services/connection_service.h"
#include "services/fx_service.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace kmx
{
    namespace detail
    {

        constexpr const char* key_direct_no_fee = "ROUTE_DIRECT_NO_FEE";
        constexpr const char* key_lowest_spread = "ROUTE_LOWEST_SPREAD";
        constexpr const char* key_two_leg = "ROUTE_TWO_LEG_VIA";
        constexpr const char* key_saves = "ROUTE_SAVES_VS_DEFAULT";

        // One desk leg, exactly per plan §6 cost model:
        //   out = round_half_away(in · mid · (1 - spread)) - fixedFee - round(out·fee%)
        // Single rounding of the full expression — no intermediate rate quantization.
        qint64 leg_payout(const fx_desk_pair_rule& rule, double mid_rate, qint64 in_minor, qint64* fee_out)
        {
            if (rule.min_ticket_minor > 0 && in_minor < rule.min_ticket_minor)
                return -1;
            if (rule.max_ticket_minor > 0 && in_minor > rule.max_ticket_minor)
                return -1;

            const double effective = mid_rate * (1.0 - rule.spread_bps / 10'000.0);
            qint64 out = static_cast<qint64>(std::llround(static_cast<double>(in_minor) * effective));

            qint64 fee = rule.fee_fixed_minor;
            if (rule.fee_bps > 0)
                fee += static_cast<qint64>(std::llround(static_cast<double>(out) * rule.fee_bps / 10'000.0));
            out -= fee;
            if (fee_out)
                *fee_out = fee;
            return out;
        }

    } // namespace detail
    exchange_advisor_service::exchange_advisor_service(account_service& accounts, connection_service& connections, fx_service& fx,
                                                       QObject* parent):
        QObject(parent),
        accounts_(accounts),
        connections_(connections),
        fx_(fx)
    {
    }

    QVector<exchange_advisor_service::venue> exchange_advisor_service::usable_venues() const
    {
        QVector<venue> out;
        for (int b = 0; b < bank_count; ++b)
        {
            venue v;
            v.id = static_cast<bank_id>(b);
            if (!connections_.is_connected(v.id))
                continue;

            // A rate-limited venue stays visible but flagged (plan §6 filter 4).
            const QString err = connections_.last_error_text(b);
            if (!err.isEmpty())
            {
                v.cooling_down = true;
                v.cooldown_note = err;
            }
            out.append(v);
        }
        return out;
    }

    qint64 exchange_advisor_service::largest_funding_at(bank_id bank, currency_code ccy, int* account_id) const
    {
        qint64 best = -1;
        int best_id = -1;
        for (const auto& a: accounts_.accounts())
        {
            if (a.bank != bank || a.currency != ccy)
                continue;
            if (a.available_minor() > best)
            {
                best = a.available_minor();
                best_id = a.id;
            }
        }
        if (account_id)
            *account_id = best_id;
        return best; // negative when the venue holds nothing in this currency
    }

    bool exchange_advisor_service::has_account_in(bank_id bank, currency_code ccy) const
    {
        for (const auto& a: accounts_.accounts())
            if (a.bank == bank && a.currency == ccy)
                return true;
        return false;
    }

    QVariantList exchange_advisor_service::advise(int src_currency, int dst_currency, qint64 amount_minor) const
    {
        QVariantList out;
        if (src_currency == dst_currency || amount_minor <= 0)
            return out;

        const currency_code src = static_cast<currency_code>(src_currency);
        const currency_code dst = static_cast<currency_code>(dst_currency);

        const auto venues = usable_venues();
        QVector<exchange_option> options;
        qint64 worst_resulting = std::numeric_limits<qint64>::max();
        bool have_any = false;

        auto consider = [&](exchange_option opt)
        {
            if (opt.resulting_minor <= 0)
                return;
            worst_resulting = std::min(worst_resulting, opt.resulting_minor);
            have_any = true;
            options.append(std::move(opt));
        };

        for (const auto& v: venues)
        {
            const fx_desk desk = connections_.fx_desk_for(static_cast<int>(v.id));
            int src_acc_id = -1;
            const qint64 funded = largest_funding_at(v.id, src, &src_acc_id);
            if (funded < amount_minor || src_acc_id < 0)
                continue; // venue cannot fund this exchange
            if (!has_account_in(v.id, dst))
                continue; // nowhere to land the proceeds

            // ---- direct route -------------------------------------------------
            if (auto rule = desk.rule_for(src, dst))
            {
                exchange_option opt;
                opt.rate_limited_venue = v.cooling_down;
                opt.rate_limit_note = v.cooldown_note;

                exchange_leg leg;
                leg.bank_id = static_cast<int>(v.id);
                leg.from = src;
                leg.to = dst;
                leg.in_minor = amount_minor;
                leg.source_account_id = src_acc_id;
                leg.applied_rate = fx_.mid(src, dst) * (1.0 - rule->spread_bps / 10'000.0);
                leg.out_minor = detail::leg_payout(*rule, fx_.mid(src, dst), amount_minor, &leg.fee_minor);
                if (leg.out_minor > 0)
                {
                    opt.legs.append(leg);
                    opt.resulting_minor = leg.out_minor;

                    if (rule->fee_bps == 0 && rule->fee_fixed_minor == 0 && !v.cooling_down)
                        opt.explanation_key = QLatin1String(detail::key_direct_no_fee);
                    else
                        opt.explanation_key = QLatin1String(detail::key_lowest_spread);
                    consider(std::move(opt));
                }
            }

            // ---- two-leg routes via RON / EUR (plan §6: these two only) --------
            for (const int mid_idx: {static_cast<int>(currency_code::ron), static_cast<int>(currency_code::eur)})
            {
                const currency_code mid = static_cast<currency_code>(mid_idx);
                if (mid == src || mid == dst)
                    continue;

                const auto leg1_rule = desk.rule_for(src, mid);
                const auto leg2_rule = desk.rule_for(mid, dst);
                if (!leg1_rule.has_value() || !leg2_rule.has_value())
                    continue;

                exchange_option opt;
                opt.rate_limited_venue = v.cooling_down;
                opt.rate_limit_note = v.cooldown_note;

                exchange_leg l1;
                l1.bank_id = static_cast<int>(v.id);
                l1.from = src;
                l1.to = mid;
                l1.in_minor = amount_minor;
                l1.source_account_id = src_acc_id;
                l1.applied_rate = fx_.mid(src, mid) * (1.0 - leg1_rule->spread_bps / 10'000.0);
                l1.out_minor = detail::leg_payout(*leg1_rule, fx_.mid(src, mid), amount_minor, &l1.fee_minor);
                if (l1.out_minor <= 0)
                    continue;

                exchange_leg l2;
                l2.bank_id = static_cast<int>(v.id);
                l2.from = mid;
                l2.to = dst;
                l2.in_minor = l1.out_minor;
                l2.applied_rate = fx_.mid(mid, dst) * (1.0 - leg2_rule->spread_bps / 10'000.0);
                l2.out_minor = detail::leg_payout(*leg2_rule, fx_.mid(mid, dst), l1.out_minor, &l2.fee_minor);
                if (l2.out_minor <= 0)
                    continue;

                opt.legs.append(l1);
                opt.legs.append(l2);
                opt.resulting_minor = l2.out_minor;
                opt.explanation_key = QLatin1String(detail::key_two_leg);
                opt.explanation_args = QVariantList {QLatin1String(to_code(mid))};
                consider(std::move(opt));
            }
        }

        if (!have_any)
            return out;

        // ---- ranking: result desc → fee asc → legs asc → stable venue order -----
        // Tie-break compares fees converted into the destination currency —
        // two-leg routes accrue fees in an intermediary currency otherwise.
        auto fee_in_dst = [&](const exchange_option& o)
        {
            qint64 total = 0;
            for (const auto& l: o.legs)
                total += fx_.convert(l.fee_minor, l.to, dst);
            return total;
        };

        std::stable_sort(options.begin(), options.end(),
                         [&](const exchange_option& a, const exchange_option& b)
                         {
                             if (a.resulting_minor != b.resulting_minor)
                                 return a.resulting_minor > b.resulting_minor;
                             const qint64 fee_a = fee_in_dst(a);
                             const qint64 fee_b = fee_in_dst(b);
                             if (fee_a != fee_b)
                                 return fee_a < fee_b;
                             return a.legs.size() < b.legs.size();
                         });

        for (int i = 0; i < options.size(); ++i)
        {
            auto& opt = options[i];
            if (i == 0)
            {
                const qint64 savings = opt.resulting_minor - worst_resulting;
                if (savings > 0)
                {
                    opt.savings_vs_worst_minor = savings;
                    opt.explanation_args.clear();
                    opt.explanation_args.append(savings);
                    if (opt.explanation_key != QLatin1String(detail::key_two_leg))
                        opt.explanation_key = QLatin1String(detail::key_saves);
                }
            }

            QVariantMap m;
            QVariantList legs;
            for (const auto& l: opt.legs)
            {
                legs.append(QVariantMap {{"bank_id", l.bank_id},
                                         {"from", static_cast<int>(l.from)},
                                         {"to", static_cast<int>(l.to)},
                                         {"in_minor", l.in_minor},
                                         {"out_minor", l.out_minor},
                                         {"applied_rate", l.applied_rate},
                                         {"fee_minor", l.fee_minor},
                                         {"source_account_id", l.source_account_id}});
            }
            m["legs"] = legs;
            m["resulting_minor"] = opt.resulting_minor;
            m["savings_vs_worst_minor"] = opt.savings_vs_worst_minor;
            m["explanation_key"] = opt.explanation_key;
            m["explanation_args"] = opt.explanation_args;
            m["rate_limited_venue"] = opt.rate_limited_venue;
            m["rate_limit_note"] = opt.rate_limit_note;
            m["recommended"] = (i == 0);
            out.append(m);
        }

        while (out.size() > max_returned_options)
            out.removeLast();
        return out;
    }

    QVariantMap exchange_advisor_service::best_route(int src_currency, int dst_currency, qint64 amount_minor) const
    {
        const QVariantList all = advise(src_currency, dst_currency, amount_minor);
        return all.isEmpty() ? QVariantMap {} : all.first().toMap();
    }

} // namespace kmx
