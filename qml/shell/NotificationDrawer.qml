import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Notification center (plan §Phase 10): severity styling, read/unread,
// deep-link on tap, mark-all-read.
Drawer {
    id: root

    required property var bank

    edge: Qt.RightEdge
    width: Math.min(380, parent ? parent.width * 0.9 : 380)
    height: parent ? parent.height : 0

    function _tone(level) {
        return level === "success" ? Theme.success
             : level === "warning" ? Theme.warning
             : level === "critical" ? Theme.danger : Theme.info
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingS

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Notifications")
                color: Theme.text
                font.pixelSize: Theme.fontSizeH3
                font.bold: true
                Layout.fillWidth: true
            }

            Button {
                flat: true
                visible: root.bank && root.bank.notifications.unread_count > 0
                text: qsTr("Mark all read")
                font.pixelSize: Theme.fontSizeCaption
                onClicked: root.bank.notifications.mark_all_read()
            }

            ToolButton {
                icon.source: Theme.icon("x")
                onClicked: root.close()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacingXS
            model: root.bank ? root.bank.notifications.items_list : []

            delegate: ItemDelegate {
                id: row
                required property int index
                required property var modelData

                width: list.width
                height: Math.max(64, contentCol.implicitHeight + 16)

                background: Rectangle {
                    radius: Theme.radiusS
                    color: row.modelData.read ? "transparent" : Theme.surfaceAlt
                    border.color: Theme.border
                }

                contentItem: RowLayout {
                    spacing: Theme.spacingS

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 4
                        radius: 2
                        color: root._tone(row.modelData.level)
                    }

                    ColumnLayout {
                        id: contentCol
                        spacing: 2
                        Layout.fillWidth: true

                        Label {
                            text: row.modelData.title
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: !row.modelData.read
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: row.modelData.body.length > 0
                            text: row.modelData.body
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Label {
                            text: Qt.formatDateTime(row.modelData.at, "dd MMM hh:mm")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }

                    Label {
                        visible: row.modelData.deep_link_key.length > 0
                        text: qsTr("Open")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }

                onClicked: {
                    const key = row.modelData.deep_link_key
                    if (!row.modelData.read) {
                        // Single-item read: cheapest correct move is mark-all.
                        root.bank.notifications.mark_all_read()
                    }
                    root.close()
                    if (key.length > 0)
                        shellRoute.open_route(key)
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: list.count === 0
                iconSource: Theme.icon("bell")
                title: qsTr("All quiet")
                message: qsTr("Sync results, alerts and security events land here.")
            }
        }
    }
}
