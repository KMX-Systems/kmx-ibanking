import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Handset top bar: back affordance, current page title, notification bell.
// Deliberately thinner than the desktop AppHeader — search and the profile
// menu live in the "More" sheet instead of competing for ~390px of width.
Item {
    id: root

    property string pageTitle: ""
    property int unread_count: 0
    property bool canGoBack: false

    signal backRequested()
    signal bellClicked()
    signal searchRequested(string text)

    // Tapping the title turns the bar into a search field, so the ledger stays
    // one gesture away without a permanent input stealing a row of screen.
    property bool searching: false

    height: FormFactor.topBarHeight

    function _closeSearch() {
        searching = false
        searchField.text = ""
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingS
        anchors.rightMargin: Theme.spacingS
        spacing: Theme.spacingXS

        ToolButton {
            visible: root.canGoBack || root.searching
            implicitWidth: FormFactor.touchTarget
            implicitHeight: FormFactor.touchTarget
            icon.source: Theme.icon("chevleft")
            icon.width: FormFactor.iconSize
            icon.height: FormFactor.iconSize
            Accessible.name: qsTr("Back")
            onClicked: root.searching ? root._closeSearch() : root.backRequested()
        }

        Label {
            visible: !root.searching
            text: root.pageTitle
            color: Theme.text
            font.pixelSize: Theme.fontSizeH3
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.leftMargin: root.canGoBack ? 0 : Theme.spacingXS
        }

        TextField {
            id: searchField
            visible: root.searching
            Layout.fillWidth: true
            placeholderText: qsTr("Search transactions…")
            font.pixelSize: Theme.fontSizeSmall
            onAccepted: {
                root.searchRequested(text)
                root._closeSearch()
            }
        }

        ToolButton {
            visible: !root.searching
            implicitWidth: FormFactor.touchTarget
            implicitHeight: FormFactor.touchTarget
            icon.source: Theme.icon("search")
            icon.width: FormFactor.iconSize
            icon.height: FormFactor.iconSize
            Accessible.name: qsTr("Search")
            onClicked: {
                root.searching = true
                searchField.forceActiveFocus()
            }
        }

        ToolButton {
            visible: !root.searching
            implicitWidth: FormFactor.touchTarget
            implicitHeight: FormFactor.touchTarget
            icon.source: Theme.icon("bell")
            icon.width: FormFactor.iconSize
            icon.height: FormFactor.iconSize
            Accessible.name: qsTr("Notifications")
            onClicked: root.bellClicked()

            Rectangle {
                visible: root.unread_count > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
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
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
}
