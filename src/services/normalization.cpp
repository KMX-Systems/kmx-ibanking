/// @file src/services/normalization.cpp
/// @brief Keyword-table category inference and merchant title-casing.
/// @copyright Copyright (C) 2026 - present KMX Systems. All rights reserved.
#include "normalization.h"
#include <QHash>
#include <QStringList>
#include <algorithm>
#include <array>

namespace kmx
{
    namespace detail
    {

        // Keyword table: first match wins. Lowercase substrings, checked against the
        // cleaned-but-untranslated merchant name.
        struct category_rule
        {
            const char* keyword;
            txn_category category;
        };

        constexpr std::array<category_rule, 18> rules {{
            {"kaufland", txn_category::groceries},
            {"lidl", txn_category::groceries},
            {"penny", txn_category::groceries},
            {"profi", txn_category::groceries},
            {"auchan", txn_category::groceries},
            {"carrefour", txn_category::groceries},
            {"petrom", txn_category::transport},
            {"rompetrol", txn_category::transport},
            {"fuel", txn_category::transport},
            {"bolt", txn_category::transport},
            {"uber", txn_category::transport},
            {"enel", txn_category::utilities},
            {"orange", txn_category::utilities},
            {"digi", txn_category::utilities},
            {"netflix", txn_category::entertainment},
            {"spotify", txn_category::entertainment},
            {"pharmacy", txn_category::health},
            {"booking", txn_category::travel},
        }};

    } // namespace detail
    QString clean_merchant_name(const QString& raw)
    {
        QString name = raw.simplified().trimmed();

        // Drop trailing location/store codes: "KAUFLAND RO 0123" -> "KAUFLAND RO"
        while (name.size() > 2 && (name.right(1) == QStringLiteral("0") || name.at(name.size() - 1).isDigit()))
            name.chop(1);
        name = name.trimmed();

        // Drop a trailing country token ("... RO").
        static const QStringList country_tokens {QStringLiteral("RO"), QStringLiteral("ROU"), QStringLiteral("ROMANIA")};
        for (const auto& token: country_tokens)
        {
            if (name.endsWith(QStringLiteral(" ") + token, Qt::CaseInsensitive))
            {
                name.chop(token.size() + 1);
                break;
            }
        }
        name = name.trimmed();

        if (name.isEmpty())
            return raw;

        // Title-case words for display.
        QStringList words = name.split(u' ');
        for (QString& w: words)
        {
            if (w.isUpper() || w.isLower()) // "KAUFLAND" or "kaufland" -> "Kaufland"
                w = w.toLower();
            if (!w.isEmpty())
                w[0] = w[0].toUpper();
        }
        return words.join(u' ');
    }

    txn_category infer_category_from_merchant(const QString& raw_name)
    {
        const QString needle = clean_merchant_name(raw_name).toLower();
        for (const auto& rule: detail::rules)
            if (needle.contains(QLatin1String(rule.keyword)))
                return rule.category;
        return txn_category::other;
    }

    const char* category_label(txn_category category)
    {
        switch (category)
        {
            case txn_category::salary:
                return "Salary";
            case txn_category::groceries:
                return "Groceries";
            case txn_category::dining:
                return "Dining";
            case txn_category::transport:
                return "Transport";
            case txn_category::utilities:
                return "Utilities";
            case txn_category::shopping:
                return "Shopping";
            case txn_category::health:
                return "Health";
            case txn_category::entertainment:
                return "Entertainment";
            case txn_category::travel:
                return "Travel";
            case txn_category::fees:
                return "Fees";
            case txn_category::transfer:
                return "Transfer";
            case txn_category::interest:
                return "Interest";
            case txn_category::fx:
                return "Fx";
            case txn_category::other:
                break;
        }
        return "Other";
    }

    bool merchant_looks_known(const QString& raw_name)
    {
        return infer_category_from_merchant(raw_name) != txn_category::other;
    }

} // namespace kmx
