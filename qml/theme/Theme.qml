pragma Singleton
import QtQuick
import QtCore

// Design tokens for KMX (plan §Phase 1): semantic dark/light palettes,
// typography scale, spacing/radius tokens and the bank brand registry.
// Pages must never hardcode hex colors — everything flows through here.
QtObject {
    id: root

    property string mode: "dark"
    readonly property bool isDark: mode === "dark"

    function toggleMode() { mode = isDark ? "light" : "dark" }

    // ---- palette ----------------------------------------------------------
    readonly property color bg:            isDark ? "#0d1117" : "#f5f7fa"
    readonly property color surface:       isDark ? "#151b23" : "#ffffff"
    readonly property color surfaceAlt:    isDark ? "#1b232d" : "#eef1f5"
    readonly property color border:        isDark ? "#26303c" : "#d8dee6"
    readonly property color text:          isDark ? "#e6edf3" : "#17222e"
    readonly property color textMuted:     isDark ? "#8fa1b3" : "#5a6b7d"

    readonly property color accent:        isDark ? "#4f8ef7" : "#16324f"
    readonly property color accentText:    "#ffffff"
    readonly property color success:       "#2ea36b"
    readonly property color warning:       "#d98f1f"
    readonly property color danger:        "#e05252"
    readonly property color info:          "#4f8ef7"

    // ---- brand registry (mirrors kmx::bank_brand_color_rgb / bank_logo_source) --
    readonly property var currencies: ["RON", "EUR", "USD"]
    function currency_code(i) { return currencies[i] }

    // Brand names are proper nouns: intentionally not translated.
    readonly property var banks: [
        { name: "KMX Bank",           rgb: 0x16324f, logo: "" },
        { name: "Banca Transilvania", rgb: 0xEC2127, logo: "qrc:/kmx/logos/bt-logo.svg" },
        { name: "TBI bank",           rgb: 0xFF6600, logo: "qrc:/kmx/logos/tbi-logo.svg" },
        { name: "Erste Bank",         rgb: 0x2870ED, logo: "qrc:/kmx/logos/erste-logo.svg" }
    ]

    function bankColor(bank_id) {
        return Qt.rgba(((banks[bank_id].rgb >> 16) & 0xff) / 255,
                       ((banks[bank_id].rgb >> 8) & 0xff) / 255,
                       (banks[bank_id].rgb & 0xff) / 255, 1)
    }

    function bankLogo(bank_id) { return banks[bank_id].logo }

    // ---- typography -------------------------------------------------------
    readonly property string fontFamily: "DejaVu Sans"
    readonly property int fontSizeH1: 28
    readonly property int fontSizeH2: 21
    readonly property int fontSizeH3: 17
    readonly property int fontSizeBody: 14
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeCaption: 11

    // Amounts render in a dedicated font so digits don't jitter while animating.
    function amountFont(pixelSize) {
        return Qt.font({ family: fontFamily,
                         pixelSize: pixelSize,
                         features: { "tnum": 1 } })
    }

    // ---- metrics ----------------------------------------------------------
    readonly property int spacingXS: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 12
    readonly property int spacingL: 16
    readonly property int spacingXL: 24
    readonly property int radiusS: 6
    readonly property int radiusM: 10
    readonly property int radiusL: 14

    readonly property int durationFast: 150
    readonly property int durationNormal: 250

    // Shell breakpoints.
    readonly property int shellCollapseBelow: 1440
    readonly property int sidebarWidth: 248
    readonly property int sidebarRailWidth: 68
    readonly property int headerHeight: 56

    // Category colors (id order mirrors kmx::TxnCategory).
    readonly property var categoryColors: [
        "#4f8ef7", // Salary
        "#2ea36b", // Groceries
        "#e07b39", // Dining
        "#8f7bf1", // Transport
        "#38bdf8", // Utilities
        "#ec4899", // Shopping
        "#ef4444", // Health
        "#f59e0b", // Entertainment
        "#14b8a6", // Travel
        "#94a3b8", // Fees
        "#60a5fa", // Transfer
        "#a3e635", // Interest
        "#fbbf24", // Fx
        "#64748b"  // Other
    ]

    // Icons (monochrome stroke set; see assets/icons).
    function icon(name) { return "qrc:/kmx/icons/" + name + ".svg" }
}
