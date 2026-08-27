import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Bottom sheet holding the destinations that do not fit the nav bar, plus the
// session actions the desktop keeps in the header profile menu.
Drawer {
    id: root

    // Entries: { key, title, icon }.
    property var entries: []
    property string currentKey: ""
    property string statusLine: ""

    signal navigate(string key)
    signal demoRequested()
    signal lockRequested()
    signal logoutRequested()

    edge: Qt.BottomEdge
    width: parent ? parent.width : 0
    height: Math.min(parent ? parent.height * 0.85 : 0,
                     sheetColumn.implicitHeight + Theme.spacingXL * 2)

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusL
        // The radius must only round the top corners; the strip covers the rest.
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Theme.radiusL
            color: Theme.surface
        }
    }

    ColumnLayout {
        id: sheetColumn
        anchors.fill: parent
        anchors.margins: Theme.spacingL
        spacing: Theme.spacingS

        Rectangle { // grab handle
            Layout.alignment: Qt.AlignHCenter
            width: 36; height: 4; radius: 2
            color: Theme.border
        }

        Repeater {
            model: root.entries

            delegate: ItemDelegate {
                id: entry
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: FormFactor.touchTarget
                onClicked: root.navigate(entry.modelData.key)

                background: Rectangle {
                    radius: Theme.radiusS
                    color: entry.modelData.key === root.currentKey
                           ? Qt.alpha(Theme.accent, Theme.isDark ? 0.18 : 0.12)
                           : "transparent"
                }

                contentItem: RowLayout {
                    spacing: Theme.spacingM

                    Image {
                        source: Theme.icon(entry.modelData.icon)
                        sourceSize: Qt.size(FormFactor.iconSize, FormFactor.iconSize)
                        opacity: entry.modelData.key === root.currentKey ? 1 : 0.7
                    }

                    Label {
                        text: entry.modelData.title
                        color: entry.modelData.key === root.currentKey ? Theme.text : Theme.textMuted
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: entry.modelData.key === root.currentKey
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        // A Flow, not a RowLayout: four labels do not fit one handset row in
        // every language, and wrapping beats eliding the actions. The wrapper is
        // what makes the wrapped height reach the layout — a Flow is asked for
        // its height before it is given the width its wrapping depends on.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: actionFlow.implicitHeight

            Flow {
                id: actionFlow
                width: parent.width
                spacing: Theme.spacingXS

                Button {
                    flat: true
                    text: Theme.isDark ? qsTr("Light") : qsTr("Dark")
                    onClicked: Theme.toggleMode()
                }
                Button {
                    flat: true
                    text: qsTr("Demo")
                    onClicked: { root.close(); root.demoRequested() }
                }
                Button {
                    flat: true
                    text: qsTr("Lock")
                    onClicked: { root.close(); root.lockRequested() }
                }
                Button {
                    flat: true
                    text: qsTr("Log out")
                    onClicked: { root.close(); root.logoutRequested() }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.statusLine
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeCaption
            elide: Text.ElideRight
        }
    }
}
