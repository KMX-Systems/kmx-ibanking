/// @file src/connectors/seed_bank_connector.h
/// @brief Shared connector base; per-bank quirks stay declarative options.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#pragma once
#include "bank_connector.h"
#include "domain/seed_world.h"
#include "services/clock_source.h"

namespace kmx
{

    /// @brief Shared implementation for connectors serving slices of the seeded world.
    /// @details Per-bank personalities come from options + tiny overrides — the quirk matrix of plan §2 stays declarative and visible in
    /// one place.
    class seed_bank_connector: public bank_connector
    {
    public:
        struct options
        {
            std::chrono::minutes session_validity {60};
            bool booked_only {false};          // bank reports only booked transactions
            bool raw_merchant_strings {false}; // counterparty arrives as "MERCHANT XX 0123"
            bool sparse_metadata {false};      // no counterparty IBANs
            enum class category_quality
            {
                native,
                inferred,
                partial
            } categories {category_quality::native};
        };

        seed_bank_connector(bank_id bank, const seed_world& world, options options, clock_source& clock);

        bank_id bank() const override { return bank_; }
        bank_capabilities capabilities() const override;
        std::expected<connector::remote_session, connector::sync_error> authenticate(
            const connector::mock_credentials& credentials) override;
        std::expected<QVector<connector::remote_account>, connector::sync_error> fetch_accounts(
            const connector::remote_session& session) override;
        std::expected<QVector<connector::remote_transaction>, connector::sync_error> fetch_transactions(
            const connector::remote_session& session, const QString& account_external_id, std::chrono::seconds since) override;

        void expire_session_for_demo() override { session_revoked_ = true; }

    protected:
        // Hook for bank-specific pre-checks (rate limits).
        virtual std::optional<connector::sync_error> pre_fetch_guard() { return std::nullopt; }

        bank_id bank_;
        clock_source& clock_;
        const seed_world& world_;
        options options_;
        bool session_revoked_ {false};

    private:
        QString external_account_id(const account& a) const;
        QString external_txn_id(const transaction& t) const;
    };

} // namespace kmx
