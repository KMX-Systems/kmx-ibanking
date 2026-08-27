import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Numeric pad for PIN-style password entry. Emits digits; the host owns the
// buffer and echo rendering.
Rectangle {
    id: root

    signal digitPressed(string d)
    signal backspacePressed()
    signal submitRequested()

    implicitWidth: grid.implicitWidth
    implicitHeight: grid.implicitHeight
    radius: Theme.radiusM
    color: Theme.surfaceAlt

    GridLayout {
        id: grid
        anchors.centerIn: parent
        columns: 3
        columnSpacing: Theme.spacingS
        rowSpacing: Theme.spacingS

        Repeater {
            model: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]

            delegate: Button {
                required property string modelData
                Layout.preferredWidth: 64
                Layout.preferredHeight: 48
                text: modelData
                font.pixelSize: Theme.fontSizeH3
                onClicked: root.digitPressed(modelData)
            }
        }

        Button {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 48
            flat: true
            icon.source: Theme.icon("x")
            icon.width: 18
            icon.height: 18
            onClicked: root.backspacePressed()
        }

        Button {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 48
            text: "0"
            font.pixelSize: Theme.fontSizeH3
            onClicked: root.digitPressed("0")
        }

        Button {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 48
            text: qsTr("Go")
            highlighted: true
            font.pixelSize: Theme.fontSizeSmall
            onClicked: root.submitRequested()
        }
    }
}
