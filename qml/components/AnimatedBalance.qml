import QtQuick
import KmxBank
import "../theme/Format.js" as Format

// Balance that rolls up to its new value instead of jumping.
// Bind `minor`; changes animate over ~700ms with tabular figures (no jitter).
Text {
    id: root

    property int minor: 0
    property string currency_code: "RON"
    property int pixelSize: Theme.fontSizeH2
    property color valueColor: Theme.text

    property real _animatedMinor: minor

    text: Format.money(Math.round(_animatedMinor), currency_code)
    color: valueColor
    font: Theme.amountFont(pixelSize)

    Behavior on _animatedMinor {
        enabled: root.visible
        NumberAnimation { duration: 700; easing.type: Easing.OutCubic }
    }
}
