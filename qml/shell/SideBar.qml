import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// App navigation. Full labels above 1440px width, icon rail below.
Rectangle {
    id: root

    property alias model: entriesRepeater.model
    property int currentIndex: 0
    // -1 = follow window width; 0/1 = explicit user override (header toggle).
    property int userCollapsed: -1 // -1 = follow window width; 0/1 = user override
    readonly property bool autoCollapsed: width < Theme.shellCollapseBelow
    readonly property bool collapsed:
        userCollapsed >= 0 ? userCollapsed === 1 : autoCollapsed

    function toggle() {
        userCollapsed = collapsed ? 0 : 1
    }

    readonly property real currentWidth: collapsed ? Theme.sidebarRailWidth : Theme.sidebarWidth
    property string footerText: ""

    signal navigate(int index)

    width: currentWidth
    Behavior on width { NumberAnimation { duration: Theme.durationNormal } }

    color: Theme.surface
    Rectangle { // right edge separator
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingXS

        Item { // brand slot
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            Text {
                visible: !root.collapsed
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                text: "KMX"
                color: Theme.accent
                font.pixelSize: Theme.fontSizeH2
                font.bold: true
            }

            BankBadge {
                anchors.centerIn: parent
                visible: root.collapsed
                bank_id: 0
                size: 30
            }
        }

        Item { Layout.preferredHeight: Theme.spacingS; Layout.fillWidth: true }

        Repeater {
            id: entriesRepeater

            delegate: ItemDelegate {
                id: entry

                required property var modelData
                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 40
                leftPadding: root.collapsed ? Math.max(0, (root.currentWidth - 20) / 2)
                                            : Theme.spacingS
                onClicked: root.navigate(entry.index)

                background: Rectangle {
                    radius: Theme.radiusS
                    color: entry.hovered ? Theme.surfaceAlt
                         : entry.index === root.currentIndex ? Qt.alpha(Theme.accent, Theme.isDark ? 0.18 : 0.12)
                         : "transparent"
                    border.width: entry.index === root.currentIndex && !entry.hovered ? 1 : 0
                    border.color: Theme.border
                }

                contentItem: Item {
                    Image {
                        id: iconImage
                        x: root.collapsed ? (parent.width - width) / 2 : 8
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon(entry.modelData.icon)
                        sourceSize: Qt.size(20, 20)
                        opacity: entry.index === root.currentIndex ? 1 : 0.75
                    }

                    Label {
                        visible: !root.collapsed
                        x: 38
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.title
                        color: entry.index === root.currentIndex ? Theme.text : Theme.textMuted
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: entry.index === root.currentIndex
                        elide: Text.ElideRight
                        width: parent.width - 46
                    }
                }

                ToolTip.visible: root.collapsed && hovered
                ToolTip.text: entry.modelData.title
                ToolTip.delay: 300
            }
        }

        Item { Layout.fillHeight: true; Layout.fillWidth: true }

        // Session status from the C++ BankingSession (proves the wiring).
        Label {
            visible: !root.collapsed
            Layout.fillWidth: true
            text: root.footerText
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeCaption
            elide: Text.ElideRight
        }
    }
}
