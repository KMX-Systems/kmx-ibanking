import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Bottom navigation. Four primary destinations plus "More"; nine tabs will not
// fit a handset, so the rest live in MoreSheet.
Rectangle {
    id: root

    // Entries: { key, title, icon }. The last slot is appended by the shell.
    property var model: []
    property string currentKey: ""
    property bool moreActive: false
    property int unread_count: 0

    signal navigate(string key)
    signal moreRequested()

    // Home-indicator / gesture-bar clearance on real handsets.
    property int bottomInset: 0

    height: FormFactor.navBarHeight + bottomInset
    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: FormFactor.navBarHeight
        spacing: 0

        Repeater {
            model: root.model

            delegate: NavButton {
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                iconName: modelData.icon
                label: modelData.title
                active: !root.moreActive && root.currentKey === modelData.key
                onTapped: root.navigate(modelData.key)
            }
        }

        NavButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "menu"
            label: qsTr("More")
            active: root.moreActive
            badge: root.unread_count
            onTapped: root.moreRequested()
        }
    }

    component NavButton: AbstractButton {
        id: nav

        property string iconName: ""
        property string label: ""
        property bool active: false
        property int badge: 0

        signal tapped()

        onClicked: nav.tapped()
        Accessible.name: nav.label

        background: Rectangle {
            color: nav.pressed ? Theme.surfaceAlt : "transparent"
        }

        contentItem: ColumnLayout {
            spacing: 2

            Item {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: FormFactor.iconSize + 8
                implicitHeight: FormFactor.iconSize

                Image {
                    anchors.centerIn: parent
                    source: Theme.icon(nav.iconName)
                    sourceSize: Qt.size(FormFactor.iconSize, FormFactor.iconSize)
                    opacity: nav.active ? 1 : 0.6
                }

                Rectangle {
                    visible: nav.badge > 0
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: -3
                    width: 8; height: 8; radius: 4
                    color: Theme.danger
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: nav.label
                color: nav.active ? Theme.accent : Theme.textMuted
                font.pixelSize: Theme.fontSizeCaption
                font.bold: nav.active
                elide: Text.ElideRight
            }
        }
    }
}
