/// @file src/domain/seed_world.cpp
/// @brief Builds the seeded accounts, transactions, cards and budgets.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "seed_world.h"
#include "domain/iban.h"
#include <QRandomGenerator>
#include <algorithm>
#include <array>

namespace kmx
{
    namespace detail
    {

        constexpr qint64 operator""_minor(unsigned long long v)
        {
            return static_cast<qint64>(v);
        }

        QDateTime day_at(const QDateTime& month_base, int day, int hour)
        {
            QDate d = month_base.date();
            d = QDate(d.year(), d.month(), std::min(day, d.daysInMonth()));
            return QDateTime(d, QTime(hour, 0, 0));
        }

        struct world_builder
        {
            explicit world_builder(const clock_source& clk, quint32 seed):
                clock(clk),
                rng(seed),
                now(clk.now()),
                month_base(now.addMonths(-11))
            {
                month_base = QDateTime(QDate(month_base.date().year(), month_base.date().month(), 1), QTime(9, 0, 0));
                world.banks.reserve(bank_count);
                for (const auto id: {bank_id::kmx_bank, bank_id::banca_transilvania, bank_id::tbi_bank, bank_id::erste_bank})
                    world.banks.append({id, QString::fromLatin1(bank_name(id)), bank_brand_color_rgb(id), bank_logo_source(id)});
            }

            const clock_source& clock;
            QRandomGenerator rng;
            QDateTime now;
            QDateTime month_base;
            QDateTime cur_month_base;
            seed_world world;
            transaction_id_t next_txn_id = 1;

            int rand_between(int lo, int hi) { return lo + static_cast<int>(rng.bounded(hi - lo + 1)); }

            // Random timestamp inside the month currently being generated,
            // always in the past.
            QDateTime post_day(int earliest_day, int latest_day, int hour = 12)
            {
                QDateTime t = day_at(cur_month_base, rand_between(earliest_day, latest_day), hour);
                if (t > now)
                    t = now.addSecs(-rand_between(3600, 86'400));
                return t;
            }

            void add(account_id_t acc, txn_direction dir, txn_category cat, category_origin src, qint64 amount_minor, QString counterparty,
                     const QDateTime& posted, txn_status status = txn_status::booked, QString iban = {}, QString fx_note = {},
                     QString reference = {})
            {
                const account* a = world.account_by_id(acc);
                transaction t;
                t.id = next_txn_id++;
                t.account_id = acc;
                t.direction = dir;
                t.category = cat;
                t.category_source = src;
                t.amount_minor = amount_minor;
                t.currency = a ? a->currency : currency_code::ron;
                t.counterparty = std::move(counterparty);
                t.counterparty_iban = std::move(iban);
                t.posted_at = posted > now ? now.addSecs(-rand_between(3600, 86'400)) : posted;
                t.status = status;
                t.fx_note = std::move(fx_note);
                t.reference = std::move(reference);
                world.transactions.append(t);
            }

            void add_accounts()
            {
                const QDateTime opened = month_base.addYears(-2);
                const auto mk = [&](account_id_t id, bank_id bank, const char* name, account_kind kind, currency_code cur,
                                    const char* bank4, qint64 bal)
                {
                    account a;
                    a.id = id;
                    a.bank = bank;
                    a.name = QString::fromLatin1(name);
                    a.kind = kind;
                    a.currency = cur;
                    a.iban = make_romanian_iban(QString::fromLatin1(bank4), 1000 + id);
                    a.balance_minor = bal;
                    a.opened_at = opened;
                    world.accounts.append(a);
                };

                mk(1, bank_id::kmx_bank, "Cont curent", account_kind::checking, currency_code::ron, "KMXB", 14'253'47_minor);
                mk(2, bank_id::kmx_bank, "Economii", account_kind::savings, currency_code::ron, "KMXB", 38'900'00_minor);
                mk(3, bank_id::kmx_bank, "Card de credit", account_kind::credit, currency_code::ron, "KMXB", -2'418'90_minor);
                mk(4, bank_id::banca_transilvania, "BT Cont principal", account_kind::checking, currency_code::ron, "BTN1", 6'320'55_minor);
                mk(5, bank_id::tbi_bank, "EUR Pocket", account_kind::checking, currency_code::eur, "TBI1", 1'845'20_minor);
                mk(6, bank_id::tbi_bank, "RON Pocket", account_kind::checking, currency_code::ron, "TBI1", 2'130'00_minor);
                mk(7, bank_id::erste_bank, "Erste EUR", account_kind::checking, currency_code::eur, "ERS1", 5'402'33_minor);
                mk(8, bank_id::erste_bank, "Travel USD", account_kind::savings, currency_code::usd, "ERS1", 780'00_minor);
            }

            void add_cards()
            {
                const auto mk = [&](qint64 id, qint64 acc, const char* label, card_network net, const char* pan, const char* exp, bool virt,
                                    qint64 limit)
                {
                    card c;
                    c.id = id;
                    c.account_id = acc;
                    c.label = QString::fromLatin1(label);
                    c.network = net;
                    c.full_pan = QString::fromLatin1(pan);
                    // Deterministic CVV from id; masked form derived from the PAN.
                    c.cvv = QStringLiteral("%1").arg((id * 7919) % 1000, 3, 10, QLatin1Char('0'));
                    c.masked_pan = c.full_pan.left(4) + QStringLiteral(" •••• •••• ") + c.full_pan.right(4);
                    c.expiry_mmyy = QString::fromLatin1(exp);
                    c.holder_name = QStringLiteral("Ana Dumitrescu");
                    c.is_virtual = virt;
                    c.daily_limit_minor = limit;
                    world.cards.append(c);
                };
                mk(1, 1, "KMX Debit", card_network::visa, "4539881204774821", "08/29", false, 300'000_minor);
                mk(2, 3, "KMX Credit", card_network::mastercard, "5412753309187760", "11/28", false, 200'000_minor);
                mk(3, 4, "BT Debit", card_network::mastercard, "5555931122040912", "03/28", false, 250'000_minor);
                mk(4, 5, "TBI Virtual EUR", card_network::visa, "4917883344013345", "01/28", true, 20'000_minor);
            }

            void add_beneficiaries()
            {
                const auto mk = [&](qint64 id, const char* name, const char* bank4, currency_code cur, bool fav)
                {
                    beneficiary b;
                    b.id = id;
                    b.name = QString::fromLatin1(name);
                    b.iban = make_romanian_iban(QString::fromLatin1(bank4), 5000 + id);
                    b.default_currency = cur;
                    b.favorite = fav;
                    b.last_used_at = {};
                    world.beneficiaries.append(b);
                };
                mk(1, "Maria Popescu (rent)", "BTN1", currency_code::ron, true);
                mk(2, "Digital Cable SRL", "KMXB", currency_code::ron, false);
                mk(3, "Andrei Ionescu", "ERS1", currency_code::ron, true);
                mk(4, "Asociatia EcoProvita", "TBI1", currency_code::ron, false);
                mk(5, "My TBI EUR Pocket", "TBI1", currency_code::eur, true);
                mk(6, "WebShop EU Ltd", "ERS1", currency_code::eur, false);
            }

            void add_budgets()
            {
                const auto mk = [&](txn_category cat, qint64 limit) { world.budgets.append({cat, limit, currency_code::ron}); };
                mk(txn_category::groceries, 220'000_minor);
                mk(txn_category::dining, 80'000_minor);
                mk(txn_category::transport, 60'000_minor);
                mk(txn_category::shopping, 90'000_minor);
                mk(txn_category::entertainment, 40'000_minor);
            }

            txn_status status_for(const QDateTime& posted)
            {
                if (posted >= now.addDays(-4))
                    return rng.bounded(100) < 45 ? txn_status::pending : txn_status::booked;
                return txn_status::booked;
            }

            // One month of activity across every account. `m` counts months back from
            // the current one (11 = oldest).
            void generate_month(int m)
            {
                const QDateTime base = month_base.addMonths(m);
                cur_month_base = base;
                constexpr std::array<std::pair<const char*, int>, 4> groceries {
                    {{"Kaufland", 6'000}, {"Lidl", 5'500}, {"Penny", 4'800}, {"Auchan Mega", 12'000}}};
                constexpr std::array<std::pair<const char*, int>, 4> dining {
                    {{"Cafe Central", 3'800}, {"Trattoria Roma", 9'000}, {"Sushi Bar Kyoto", 15'000}, {"Burger Truck", 4'200}}};
                constexpr std::array<std::pair<const char*, int>, 3> transport {
                    {{"Petrom fuel", 18'000}, {"STB transit", 600}, {"Bolt ride", 2'400}}};

                // ---- KMX RON checking (#1) --------------------------------------
                add(1, txn_direction::credit, txn_category::salary, category_origin::native, 850'000_minor, "NordTech SRL salary",
                    day_at(base, std::min(5, now.date().day()), 8), status_for(day_at(base, 5, 8)));
                add(1, txn_direction::debit, txn_category::other, category_origin::native, 260'000_minor, "Maria Popescu (rent)",
                    day_at(base, 1, 10), txn_status::booked, world.beneficiaries.at(0).iban);
                add(1, txn_direction::debit, txn_category::entertainment, category_origin::native, 3'900_minor, "Netflix",
                    day_at(base, 8, 9));
                add(1, txn_direction::debit, txn_category::entertainment, category_origin::native, 2'100_minor, "Spotify",
                    day_at(base, 12, 9));
                add(1, txn_direction::debit, txn_category::utilities, category_origin::native, 6'500_minor, "Digital Cable SRL",
                    day_at(base, 15, 10));

                for (int i = rand_between(4, 7); i > 0; --i)
                {
                    const auto& g = groceries[rand_between(0, 3)];
                    add(1, txn_direction::debit, txn_category::groceries, category_origin::native, g.second + rand_between(-1'500, 26'000),
                        g.first, post_day(2, 28, 18), txn_status::booked);
                }
                for (int i = rand_between(4, 7); i > 0; --i)
                {
                    const auto& d = dining[rand_between(0, 3)];
                    add(1, txn_direction::debit, txn_category::dining, category_origin::native, d.second + rand_between(-1'000, 6'000),
                        d.first, post_day(2, 28, 20));
                }
                for (int i = rand_between(5, 9); i > 0; --i)
                {
                    const auto& t = transport[rand_between(0, 2)];
                    add(1, txn_direction::debit, txn_category::transport, category_origin::native, t.second + rand_between(-400, 4'000),
                        t.first, post_day(2, 28, 17));
                }
                if (rng.bounded(100) < 60)
                {
                    add(1, txn_direction::debit, txn_category::shopping, category_origin::native, rand_between(8'000, 45'000), "eMag",
                        post_day(3, 27, 21));
                }
                if (rng.bounded(100) < 30)
                {
                    add(1, txn_direction::debit, txn_category::health, category_origin::native, rand_between(5'000, 30'000),
                        "Pharmacy Help", post_day(2, 28, 16));
                }
                if (rng.bounded(100) < 70)
                {
                    add(1, txn_direction::debit, txn_category::dining, category_origin::native, rand_between(4'000, 9'000),
                        "Glovo delivery", post_day(2, 28, 20));
                }
                add(1, txn_direction::debit, txn_category::transfer, category_origin::native, 50'000_minor, "Credit card payment",
                    day_at(base, std::min(20, now.date().day()), 11), status_for(day_at(base, 20, 11)), world.accounts.at(2).iban);

                // ---- KMX savings (#2): monthly interest --------------------------
                add(2, txn_direction::credit, txn_category::interest, category_origin::native, 13'000_minor, "Savings interest",
                    day_at(base, 28, 23));

                // ---- KMX credit card (#3): spend + repayment --------------------
                for (int i = rand_between(8, 12); i > 0; --i)
                {
                    const auto pick = rand_between(0, 2);
                    const auto cat = pick == 0 ? txn_category::groceries : pick == 1 ? txn_category::dining : txn_category::shopping;
                    const QDateTime posted = post_day(1, 28, 19);
                    add(3, txn_direction::debit, cat, category_origin::native, rand_between(4'000, 60'000), "Card purchase", posted,
                        status_for(posted));
                }
                add(3, txn_direction::credit, txn_category::transfer, category_origin::native, 50'000_minor, "Payment received",
                    day_at(base, std::min(22, now.date().day()), 11));

                // ---- BT RON checking (#4): raw strings, inferred categories -----
                add(4, txn_direction::credit, txn_category::salary, category_origin::inferred, 220'000_minor, "FREELANCE PAYOUT EU",
                    day_at(base, std::min(10, now.date().day()), 14));
                add(4, txn_direction::debit, txn_category::utilities, category_origin::inferred, 18'000_minor, "ENEL ENERGIE RO",
                    day_at(base, 18, 10));
                add(4, txn_direction::debit, txn_category::utilities, category_origin::inferred, 4'500_minor, "DIGI COMMUNICATIONS",
                    day_at(base, 3, 10));
                for (int i = rand_between(2, 4); i > 0; --i)
                {
                    add(4, txn_direction::debit, txn_category::groceries, category_origin::inferred, rand_between(4'000, 22'000),
                        "PROFI ROMANIA", post_day(2, 28, 18));
                }
                add(4, txn_direction::debit, txn_category::transport, category_origin::inferred, rand_between(12'000, 22'000),
                    "ROMPETROL VIA", post_day(6, 26, 18));
                add(4, txn_direction::debit, txn_category::utilities, category_origin::inferred, 3'500_minor, "ORANGE RO",
                    day_at(base, 11, 10));

                // ---- Erste EUR (#7) ----------------------------------------------
                add(7, txn_direction::credit, txn_category::salary, category_origin::native, 90'000_minor, "Consulting invoice",
                    day_at(base, std::min(7, now.date().day()), 13));
                add(7, txn_direction::debit, txn_category::transfer, category_origin::native, 30'000_minor, "To TBI EUR Pocket",
                    day_at(base, 2, 9), txn_status::booked, world.accounts.at(4).iban);
                if (m % 6 == 0)
                {
                    add(7, txn_direction::debit, txn_category::travel, category_origin::native, rand_between(120'000, 380'000),
                        "Booking.com", post_day(5, 25, 15));
                }
                add(7, txn_direction::debit, txn_category::shopping, category_origin::native, rand_between(3'000, 12'000), "Amazon DE",
                    post_day(2, 28, 21));
                for (int i = rand_between(2, 4); i > 0; --i)
                {
                    add(7, txn_direction::debit, txn_category::dining, category_origin::native, rand_between(2'500, 9'000),
                        "Wienerwald Cafe", post_day(3, 27, 19));
                }

                // ---- TBI EUR pocket (#5): FX buys land here ---------------------
                add(5, txn_direction::credit, txn_category::fx, category_origin::native, 29'850_minor, "Currency exchange EUR/RON",
                    day_at(base, std::min(3, now.date().day()), 9), txn_status::booked, {}, QStringLiteral("1 RON = 0.2011 EUR"));
                for (int i = rand_between(4, 7); i > 0; --i)
                {
                    add(5, txn_direction::debit, txn_category::other, category_origin::native, rand_between(999, 2'999),
                        "App Store & iCloud", post_day(2, 28, 22));
                }

                // ---- TBI RON pocket (#6): FX sales + coffee ---------------------
                add(6, txn_direction::credit, txn_category::fx, category_origin::native, rand_between(48'000, 62'000),
                    "Currency exchange RON/EUR", day_at(base, std::min(3, now.date().day()), 9), txn_status::booked, {},
                    QStringLiteral("1 EUR = 4.9723 RON"));
                for (int i = rand_between(6, 12); i > 0; --i)
                {
                    add(6, txn_direction::debit, txn_category::dining, category_origin::native, rand_between(1'200, 2'800),
                        "5 To Go coffee", post_day(1, 28, 9));
                }

                // ---- Erste USD travel (#8) ---------------------------------------
                if (m % 4 == 1)
                {
                    add(8, txn_direction::credit, txn_category::fx, category_origin::native, rand_between(20'000, 45'000),
                        "Currency exchange USD/EUR", day_at(base, 9, 12), txn_status::booked, {}, QStringLiteral("1 EUR = 1.0842 USD"));
                }
                for (int i = rand_between(1, 2); i > 0; --i)
                {
                    add(8, txn_direction::debit, txn_category::transport, category_origin::native, rand_between(6'000, 12'000),
                        "ATM withdrawal", post_day(2, 28, 19));
                }
            }
        };

    } // namespace detail
    seed_world generate_seed_world(const clock_source& clock, quint32 seed)
    {
        detail::world_builder builder(clock, seed);
        builder.add_accounts();
        builder.add_cards();
        builder.add_beneficiaries();
        builder.add_budgets();

        for (int m = 11; m >= 0; --m)
            builder.generate_month(m);

        std::sort(builder.world.transactions.begin(), builder.world.transactions.end(),
                  [](const transaction& a, const transaction& b) { return a.posted_at > b.posted_at; });

        return std::move(builder.world);
    }

} // namespace kmx
