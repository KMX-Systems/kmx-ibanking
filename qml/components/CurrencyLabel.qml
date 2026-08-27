import QtQuick
import KmxBank
import "../theme/Format.js" as Format

// Formatted money amount. `minor` is integer minor units; doubles never enter.
// tone: "auto" colors negatives as danger / positive credits as success,
//       "neutral" keeps the plain text color.
Text {
    id: root

    property int minor: 0
    property string currency_code: "RON"
    property string tone: "auto"
    property bool signedDisplay: false
    property int pixelSize: Theme.fontSizeBody

    text: signedDisplay && minor > 0
          ? Format.signedMoney(minor, currency_code)
          : Format.money(minor, currency_code)

    color: {
        if (tone === "neutral")
            return Theme.text
        if (minor < 0)
            return Theme.danger
        if (minor > 0 && signedDisplay)
            return Theme.success
        return Theme.text
    }
    font: Theme.amountFont(pixelSize)
}
