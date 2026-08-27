/// @file src/services/payment_service.cpp
/// @brief Validation catalog, execution legs and the standing-order queue.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "payment_service.h"
#include "domain/iban.h"
#include "services/account_service.h"
#include "services/card_service.h"
#include "services/exchange_advisor_service.h"
#include <QTimer>
#include <algorithm>

namespace kmx
{

    namespace detail
    {
        payment_error fail(payment_error_code code, const QString& message)
        {
            return payment_error {code, message};
        }
    } // namespace detail
    payment_service::payment_service(account_service& accounts, clock_source& clock, QObject* parent):
        QObject(parent),
        accounts_(accounts),
        clock_(clock)
    {
    }

    const account* payment_service::find_account(qint64 id) const
    {
        for (const auto& a: accounts_.accounts())
            if (a.id == id)
                return &a;
        return nullptr;
    }

    qint64 payment_service::resolve_internal_destination(const QString& iban) const
    {
        for (const auto& a: accounts_.accounts())
            if (QString::compare(a.iban, iban, Qt::CaseInsensitive) == 0)
                return a.id;
        return -1;
    }

    qint64 payment_service::find_largest_at(int bank_id, currency_code ccy, qint64 exclude_account_id) const
    {
        qint64 best = -1, best_id = -1;
        for (const auto& a: accounts_.accounts())
        {
            if (static_cast<int>(a.bank) != bank_id || a.currency != ccy || a.id == exclude_account_id)
                continue;
            if (a.available_minor() > best)
            {
                best = a.available_minor();
                best_id = a.id;
            }
        }
        return best_id;
    }

    std::expected<validated_transfer, payment_error> payment_service::validate(const transfer_request& request) const
    {
        // --- amount -----------------------------------------------------------
        if (request.amount_minor < min_transfer_minor)
            return std::unexpected(detail::fail(payment_error_code::amount_too_small,
                                                QObject::tr("Minimum transfer amount is %1.").arg(min_transfer_minor / 100.0, 0, 'f', 2)));

        if (request.amount_minor > max_transfer_minor)
            return std::unexpected(detail::fail(payment_error_code::amount_too_large,
                                                QObject::tr("Maximum transfer amount is %1.").arg(max_transfer_minor / 100.0, 0, 'f', 2)));

        // --- source -----------------------------------------------------------
        const account* source = find_account(request.source_account_id);
        if (!source)
            return std::unexpected(detail::fail(payment_error_code::validation_required, QObject::tr("Pick a source account.")));

        // card-linked guards (plan §Phase 8): a frozen debit card blocks the
        // account as a transfer source; daily limits cap today's outflows.
        if (cards_)
        {
            if (cards_->account_frozen(request.source_account_id))
                return std::unexpected(
                    detail::fail(payment_error_code::validation_required, QObject::tr("This account is blocked: its card is frozen.")));

            const qint64 limit = cards_->daily_limit_for_account(request.source_account_id);
            if (limit > 0)
            {
                const QDateTime day_start(
                    [this]
                    {
                        QDate d = clock_.now().date();
                        return QDateTime(d, QTime(0, 0));
                    }());
                qint64 today_out = 0;
                for (const auto& t: accounts_.transactions())
                {
                    if (t.account_id != request.source_account_id || t.direction != txn_direction::debit || t.posted_at < day_start)
                        continue;
                    today_out += t.amount_minor;
                }
                if (today_out + request.amount_minor > limit)
                    return std::unexpected(
                        detail::fail(payment_error_code::validation_required, QObject::tr("Daily card limit exceeded for this account.")));
            }
        }

        if (source->available_minor() < request.amount_minor)
            return std::unexpected(
                detail::fail(payment_error_code::insufficient_funds, QObject::tr("Insufficient funds on %1.").arg(source->name)));

        // --- destination IBAN --------------------------------------------------
        const QString iban = request.beneficiary_iban;
        if (iban.isEmpty())
            return std::unexpected(detail::fail(payment_error_code::validation_required, QObject::tr("Enter a destination IBAN.")));

        if (!is_valid_iban(iban))
            return std::unexpected(detail::fail(payment_error_code::invalid_iban, QObject::tr("IBAN failed the checksum test.")));

        if (!iban.startsWith(QStringLiteral("RO"), Qt::CaseInsensitive))
            return std::unexpected(detail::fail(payment_error_code::invalid_iban, QObject::tr("This demo supports Romanian IBANs only.")));

        const qint64 dest_id = resolve_internal_destination(iban);
        if (dest_id >= 0 && dest_id == request.source_account_id)
            return std::unexpected(
                detail::fail(payment_error_code::same_source_and_destination, QObject::tr("Source and destination are the same account.")));

        // --- currency rules until the P7 exchange advisor -----------------------
        if (dest_id >= 0)
        {
            const account* dest = find_account(dest_id);
            if (dest && dest->currency != source->currency)
                return std::unexpected(detail::fail(payment_error_code::currency_pair_unsupported,
                                                    QObject::tr("No funded route for this pair yet — connect a bank "
                                                                "that holds both currencies.")));
        }
        else if (source->currency != currency_code::ron)
        {
            return std::unexpected(detail::fail(payment_error_code::currency_pair_unsupported,
                                                QObject::tr("No funded route from a %1 account yet — connect a bank "
                                                            "that can land this payment.")
                                                    .arg(QLatin1String(to_code(source->currency)))));
        }

        validated_transfer out;
        out.request = request;
        out.destination_account_id = dest_id;
        out.source_currency = source->currency;
        return out;
    }

    void payment_service::execute(const validated_transfer& validated)
    {
        // Zero latency => synchronous execution so unit tests need no event loop.
        if (latency_.count() <= 0)
        {
            finish_execution(validated);
            return;
        }
        QTimer::singleShot(latency_, this, [this, validated] { finish_execution(validated); });
    }

    void payment_service::finish_execution(const validated_transfer& v)
    {
        const account* source = find_account(v.request.source_account_id);
        if (!source)
        { // account vanished mid-flight: extremely defensive
            emit transfer_failed(QStringLiteral("VALIDATION_REQUIRED"), QObject::tr("Source account is gone; transfer cancelled."));
            return;
        }

        const bool internal = v.destination_account_id >= 0;
        const account* destination = internal ? find_account(v.destination_account_id) : nullptr;
        if (internal && !destination)
        {
            emit transfer_failed(QStringLiteral("VALIDATION_REQUIRED"), QObject::tr("Destination account is gone; transfer cancelled."));
            return;
        }

        QString reference;

        if (!accounts_.post_local_transaction(v.request.source_account_id, txn_direction::debit,
                                              internal ? destination->name : v.request.beneficiary_name, v.request.beneficiary_iban,
                                              v.request.amount_minor, v.request.note, &reference))
        {
            emit transfer_failed(QStringLiteral("VALIDATION_REQUIRED"), QObject::tr("Could not post the transfer."));
            return;
        }

        if (internal)
        {
            accounts_.post_local_transaction(v.destination_account_id, txn_direction::credit, source->name, source->iban,
                                             v.request.amount_minor, QStringLiteral("From %1 · %2").arg(source->name, reference));
        }

        transfer_receipt receipt;
        receipt.reference = reference;
        receipt.booked_at = clock_.now();
        receipt.source_name = source->name;
        receipt.beneficiary_name = internal ? destination->name : v.request.beneficiary_name;
        receipt.amount_minor = v.request.amount_minor;
        receipt.currency = v.source_currency;
        receipt.internal_ = internal;

        QVariantMap r;
        r["reference"] = receipt.reference;
        r["booked_at"] = receipt.booked_at;
        r["source_name"] = receipt.source_name;
        r["beneficiary_name"] = receipt.beneficiary_name;
        r["amount_minor"] = receipt.amount_minor;
        r["currency_code"] = QLatin1String(to_code(receipt.currency));
        r["internal"] = receipt.internal_;

        emit transfer_completed(r);
    }

    bool payment_service::submit(const QVariantMap& m)
    {
        transfer_request req;
        req.source_account_id = m.value("source_account_id").toLongLong();
        req.beneficiary_iban = m.value("beneficiary_iban").toString();
        req.beneficiary_name = m.value("beneficiary_name").toString();
        req.amount_minor = m.value("amount_minor").toLongLong();
        req.note = m.value("note").toString();

        auto v = validate(req);
        if (!v.has_value())
        {
            // Cross-currency gets a second chance through the advisor (P7).
            if (v.error().code == payment_error_code::currency_pair_unsupported && advisor_)
            {
                const account* src = find_account(req.source_account_id);
                const qint64 dest_id = resolve_internal_destination(req.beneficiary_iban);
                if (dest_id >= 0)
                {
                    const account* dst = find_account(dest_id);
                    if (src && dst)
                    {
                        const auto route =
                            advisor_->best_route(static_cast<int>(src->currency), static_cast<int>(dst->currency), req.amount_minor);
                        if (!route.isEmpty())
                        {
                            execute_exchange(route);
                            return true;
                        }
                    }
                }
            }
            emit transfer_failed(QStringLiteral("PAYMENT_ERROR"), v.error().message);
            return false;
        }
        execute(v.value());
        return true;
    }

    bool payment_service::execute_exchange(const QVariantMap& route)
    {
        const auto legs = route.value("legs").toList();
        if (legs.isEmpty() || !advisor_)
        {
            emit transfer_failed(QStringLiteral("VALIDATION_REQUIRED"), QObject::tr("No exchange route selected."));
            return false;
        }

        qint64 running = 0;
        qint64 last_from_id = -1;

        for (int i = 0; i < legs.size(); ++i)
        {
            const QVariantMap leg = legs[i].toMap();
            const int bank_id = leg.value("bank_id").toInt();
            const currency_code from_ccy = static_cast<currency_code>(leg.value("from").toInt());
            const currency_code to_ccy = static_cast<currency_code>(leg.value("to").toInt());

            const qint64 amount_in = (i == 0) ? leg.value("in_minor").toLongLong() : running;
            const qint64 amount_out = leg.value("out_minor").toLongLong();

            // Debit leg: first leg debits the funded account; later legs Debit the
            // intermediary account credited in the previous step.
            qint64 from_acc_id = (i == 0) ? leg.value("source_account_id").toLongLong() : find_largest_at(bank_id, from_ccy, last_from_id);

            if (from_acc_id < 0 || !find_account(from_acc_id))
            {
                emit transfer_failed(QStringLiteral("VALIDATION_REQUIRED"),
                                     QObject::tr("Missing %1 pocket at this bank for the exchange.").arg(QLatin1String(to_code(from_ccy))));
                return false;
            }
            if (find_account(from_acc_id)->available_minor() < amount_in)
            {
                emit transfer_failed(QStringLiteral("INSUFFICIENT_FUNDS"), QObject::tr("Funds changed while exchanging."));
                return false;
            }

            // Credit target: user's account in the destination currency at this venue.
            qint64 to_acc_id = -1;
            if (i == legs.size() - 1)
                to_acc_id = find_largest_at(bank_id, to_ccy, from_acc_id);
            else
                to_acc_id = find_largest_at(bank_id, to_ccy, -1);
            if (to_acc_id < 0)
            {
                emit transfer_failed(
                    QStringLiteral("VALIDATION_REQUIRED"),
                    QObject::tr("Open a %1 pocket at this bank to land the exchange.").arg(QLatin1String(to_code(to_ccy))));
                return false;
            }

            const QString fx_note = QStringLiteral("1 %1 = %2 %3")
                                        .arg(QLatin1String(to_code(from_ccy)))
                                        .arg(leg.value("applied_rate").toDouble(), 0, 'f', 4)
                                        .arg(QLatin1String(to_code(to_ccy)));

            QString ref;
            accounts_.post_local_transaction(from_acc_id, txn_direction::debit, QStringLiteral("Currency exchange"), {}, amount_in, {},
                                             &ref, txn_category::fx, fx_note);
            accounts_.post_local_transaction(to_acc_id, txn_direction::credit, QStringLiteral("Currency exchange"), {}, amount_out, {},
                                             nullptr, txn_category::fx, fx_note);

            running = amount_out;
            last_from_id = to_acc_id;
        }

        QVariantMap receipt = route;
        receipt["reference"] = QStringLiteral("FX-%1").arg(clock_.now().toString(QStringLiteral("hhmmss")));
        emit transfer_completed(receipt);
        return true;
    }

    QVariantMap payment_service::schedule_from_map(const QVariantMap& m, int day_of_month)
    {
        transfer_request req;
        req.source_account_id = m.value("source_account_id").toLongLong();
        req.beneficiary_iban = m.value("beneficiary_iban").toString();
        req.beneficiary_name = m.value("beneficiary_name").toString();
        req.amount_minor = m.value("amount_minor").toLongLong();
        req.note = m.value("note").toString();

        auto result = schedule(req, day_of_month);
        if (result.has_value())
            return QVariantMap {{"ok", true}, {"id", result.value()}};
        return QVariantMap {{"ok", false}, {"code", QStringLiteral("PAYMENT_ERROR")}, {"message", result.error().message}};
    }

    QVariantList payment_service::scheduled_list() const
    {
        QVariantList out;
        for (const auto& s: scheduled_)
        {
            out.append(QVariantMap {{"id", s.id},
                                    {"beneficiary_name", s.beneficiary_name},
                                    {"amount_minor", s.amount_minor},
                                    {"day_of_month", s.day_of_month},
                                    {"next_run", s.next_run}});
        }
        return out;
    }

    // ---- scheduled payments -------------------------------------------------

    scheduled_payment* payment_service::find_scheduled(qint64 id)
    {
        for (auto& s: scheduled_)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    std::expected<qint64, payment_error> payment_service::schedule(const transfer_request& request, int day_of_month)
    {
        // Validate exactly like an immediate transfer.
        auto validation = validate(request);
        if (!validation.has_value())
            return std::unexpected(validation.error());

        const account* source = find_account(request.source_account_id);
        const int bank_slot = static_cast<int>(source->bank);

        if (!capabilities_(bank_slot))
            return std::unexpected(detail::fail(payment_error_code::capability_missing,
                                                QObject::tr("%1 does not support scheduled payments.")
                                                    .arg(QString::fromLatin1(bank_name(static_cast<kmx::bank_id>(bank_slot))))));

        scheduled_payment s;
        s.id = next_schedule_id_++;
        s.source_account_id = request.source_account_id;
        s.beneficiary_iban = request.beneficiary_iban;
        s.beneficiary_name = request.beneficiary_name;
        s.amount_minor = request.amount_minor;
        s.day_of_month = std::clamp(day_of_month, 1, 28);
        s.note = request.note;

        QDate next = clock_.now().date();
        next = QDate(next.year(), next.month(), s.day_of_month);
        if (next <= clock_.now().date())
            next = next.addMonths(1);
        s.next_run = next;

        scheduled_.append(s);
        emit scheduled_changed();
        return s.id;
    }

    bool payment_service::cancel_scheduled(qint64 id)
    {
        const auto before = scheduled_.size();
        scheduled_.erase(std::remove_if(scheduled_.begin(), scheduled_.end(), [id](const scheduled_payment& s) { return s.id == id; }),
                         scheduled_.end());
        if (scheduled_.size() != before)
        {
            emit scheduled_changed();
            return true;
        }
        return false;
    }

    int payment_service::run_due_now(bool force)
    {
        int executed = 0;
        for (const auto& s: scheduled_)
        {
            if (!force && s.next_run > clock_.now().date())
                continue;

            transfer_request req;
            req.source_account_id = s.source_account_id;
            req.beneficiary_iban = s.beneficiary_iban;
            req.beneficiary_name = s.beneficiary_name;
            req.amount_minor = s.amount_minor;
            req.note = s.note.isEmpty() ? QStringLiteral("Standing order") : s.note;

            scheduled_payment* row = find_scheduled(s.id);
            const QDate next = QDate(s.next_run.year(), s.next_run.month(), s.day_of_month).addMonths(1);

            auto validation = validate(req);
            if (!validation.has_value())
            {
                // Underfunded/blocked: skip this cycle but stop retrying every tick.
                if (row)
                    row->next_run = next;
                continue;
            }

            finish_execution(validation.value());
            if (row)
                row->next_run = next;
            ++executed;
        }
        return executed;
    }

    void payment_service::tick()
    {
        const QDate today = clock_.now().date();
        for (const auto& s: scheduled_)
            if (s.next_run <= today)
            {
                run_due_now();
                break;
            }
    }

} // namespace kmx
