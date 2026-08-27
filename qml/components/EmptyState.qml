import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Standard empty state: icon, title, explanation and an optional action.
ColumnLayout {
    id: root

    property string iconSource: Theme.icon("inbox")
    property string title: qsTr("Nothing here yet")
    property string message: ""
    property string actionText: ""
    signal actionTriggered()

    spacing: Theme.spacingM

    Image {
        Layout.alignment: Qt.AlignHCenter
        source: root.iconSource
        sourceSize: Qt.size(44, 44)
        opacity: 0.7
    }

    // Keeps a paragraph readable on a wide canvas without letting it dictate a
    // minimum width the enclosing layout then refuses to shrink below.
    readonly property int maxTextWidth: 420

    Label {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maxTextWidth
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontSizeH3
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    Label {
        Layout.fillWidth: true
        Layout.maximumWidth: root.maxTextWidth
        Layout.alignment: Qt.AlignHCenter
        visible: root.message.length > 0
        text: root.message
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeSmall
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    Button {
        Layout.alignment: Qt.AlignHCenter
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.actionTriggered()
    }
}
