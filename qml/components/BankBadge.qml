import QtQuick
import KmxBank

// Mandatory brand chip shown next to every bank-owned surface (plan rule 7):
// colored ring + logo where available, initial-letter monogram otherwise.
Rectangle {
    id: root

    property int bank_id: 0
    property real size: 26

    width: size
    height: size
    radius: size / 2
    color: Qt.lighter(Theme.bankColor(bank_id), Theme.isDark ? 1.9 : 1.25)
    border.color: Theme.bankColor(bank_id)
    border.width: 1

    Text {
        anchors.centerIn: parent
        visible: !logoImage.visible
        text: Theme.banks[root.bank_id].name.charAt(0)
        color: Theme.bankColor(root.bank_id)
        font.pixelSize: root.size * 0.5
        font.bold: true
    }

    Image {
        id: logoImage
        anchors.fill: parent
        anchors.margins: parent.height * 0.22
        visible: source.toString().length > 0
        source: Theme.bankLogo(root.bank_id)
        fillMode: Image.PreserveAspectFit
        mipmap: true
    }
}
