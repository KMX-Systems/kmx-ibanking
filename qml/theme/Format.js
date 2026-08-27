// Shared money/date formatting helpers. Locale-aware via Qt.locale();
// amounts are always integer minor units — doubles never touch money.
.pragma library

function money(minor, currencyCode, localeName) {
    var loc = localeName && localeName.length ? Qt.locale(localeName) : Qt.locale()
    var negative = minor < 0
    var value = Math.abs(minor) / 100
    var s = value.toLocaleString(loc, "f", 2)
    return (negative ? "\u2212" : "") + s + " " + currencyCode
}

function signedMoney(minor, currencyCode, localeName) {
    if (minor > 0)
        return "+" + money(minor, currencyCode, localeName)
    return money(minor, currencyCode, localeName)
}

function percent(fraction, localeName) {
    var loc = localeName && localeName.length ? Qt.locale(localeName) : Qt.locale()
    return (fraction * 100).toLocaleString(loc, "f", 0) + "%"
}

function monthName(year, month) {
    const loc = Qt.locale()
    return loc.standaloneMonthName(month, 1) || ("M" + month)
}
