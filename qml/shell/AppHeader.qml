import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Top bar: menu toggle, current page title, search stub, theme toggle,
// notifications bell and profile menu.
Item {
    id: root

    property string pageTitle: ""
    property bool sidebarCollapsed: false
    property int unread_count: 0

    signal toggleSidebar()
    signal searchRequested(string text)
    signal bellClicked()
    signal lockRequested()
    signal logoutRequested()
    signal profileRequested()

    height: Theme.headerHeight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingM
        anchors.rightMargin: Theme.spacingM
        spacing: Theme.spacingS

        ToolButton {
            icon.source: Theme.icon("menu")
            icon.width: 20
            icon.height: 20
            onClicked: root.toggleSidebar()
            Accessible.name: qsTr("Toggle navigation")
        }

        Label {
            text: root.pageTitle
            color: Theme.text
            font.pixelSize: Theme.fontSizeH3
            font.bold: true
            Layout.fillWidth: true
        }

        // Jumps to the full ledger search (per-page filter lives there).
        TextField {
            Layout.preferredWidth: 240
            placeholderText: qsTr("Search transactions…")
            font.pixelSize: Theme.fontSizeSmall
            onAccepted: function () {
                root.searchRequested(text)
                text = ""
            }
        }

        Button {
            id: themeToggle
            flat: true
            text: Theme.isDark ? qsTr("Light") : qsTr("Dark")
            font.pixelSize: Theme.fontSizeSmall
            onClicked: Theme.toggleMode()
        }

        // Notifications bell — badge appears once NotificationService lands.
        ToolButton {
            icon.source: Theme.icon("bell")
            icon.width: 20
            icon.height: 20
            Accessible.name: qsTr("Notifications")
            onClicked: root.bellClicked()

            Rectangle {
                visible: root.unread_count > 0
                anchors.right: parent.right
                anchors.top: parent.top
                width: 16; height: 16; radius: 8
                color: Theme.danger
                Label {
                    anchors.centerIn: parent
                    text: root.unread_count > 9 ? "9+" : root.unread_count
                    color: "#fff"
                    font.pixelSize: 9
                }
            }
        }

        Button {
            id: profileButton
            flat: true
            leftPadding: 6
            contentItem: Row {
                spacing: 6
                Rectangle {
                    width: 26; height: 26; radius: 13
                    color: Theme.accent
                    Label {
                        anchors.centerIn: parent
                        text: "AD"
                        color: Theme.accentText
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                    }
                }
            }
            onClicked: profileMenu.popup()

            Menu {
                id: profileMenu
                width: 190

                MenuItem { text: qsTr("Profile");           onTriggered: root.profileRequested() }
                MenuItem { text: qsTr("Lock session");      onTriggered: root.lockRequested() }
                MenuSeparator {}
                MenuItem { text: qsTr("Log out");           onTriggered: root.logoutRequested() }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
}
