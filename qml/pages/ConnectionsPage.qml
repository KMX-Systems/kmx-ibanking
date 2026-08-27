import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Linked-bank management (plan §Phase 3): per-bank status, manual refresh
// with inline rate-limit feedback, reconnect, disconnect, and a demo hook to
// force Banca Transilvania's session expiry.
Page {
    id: root

    required property var bank

    background: null

    function _stateText(stateInt) {
        switch (stateInt) {
        case 2: return qsTr("Connected")
        case 3: return qsTr("Needs re-authentication")
        case 4: return qsTr("Sync error")
        case 1: return qsTr("Authenticating…")
        default: return qsTr("Not connected")
        }
    }

    function _stateColor(stateInt) {
        switch (stateInt) {
        case 2: return Theme.success
        case 3: return Theme.warning
        case 4: return Theme.danger
        default: return Theme.textMuted
        }
    }

    title: qsTr("Connections")

    header: SectionHeader {
        title: root.title
        pageHeader: true
        actionText: qsTr("Connect a bank")
        onActionTriggered: connectFlow.openFlow()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        spacing: Theme.spacingM

        Repeater {
            model: 4

            delegate: Frame {
                id: bankCard

                required property int index

                readonly property int st:
                    root.bank.connections.state(bankCard.index)
                readonly property bool syncing:
                    root.bank.connections.is_syncing(bankCard.index)
                readonly property string lastError:
                    root.bank.connections.last_error_text(bankCard.index)
                readonly property bool isBt: bankCard.index === 1 // BancaTransilvania

                Layout.fillWidth: true

                // Identity beside the actions on a desktop; stacked, with the
                // actions wrapping, once the row would exceed the canvas.
                contentItem: GridLayout {
                    columns: FormFactor.compact ? 1 : 2
                    columnSpacing: Theme.spacingM
                    rowSpacing: Theme.spacingS

                    RowLayout {
                        spacing: Theme.spacingM
                        Layout.fillWidth: true

                        BankBadge { bank_id: bankCard.index; size: 34 }

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Label {
                                text: Theme.banks[bankCard.index].name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeBody
                                font.bold: true
                            }

                            RowLayout {
                                spacing: Theme.spacingXS

                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: bankCard.syncing ? Theme.info
                                           : root._stateColor(bankCard.st)
                                }

                                Label {
                                    text: bankCard.syncing ? qsTr("Syncing…")
                                          : root._stateText(bankCard.st)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSmall
                                }

                                Label {
                                    visible: !bankCard.syncing && bankCard.st === 2 &&
                                             !isNaN(root.bank.connections.last_sync_at(bankCard.index).getTime())
                                    text: "· " + Qt.formatTime(
                                              root.bank.connections.last_sync_at(bankCard.index),
                                              "hh:mm:ss")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            Label {
                                visible: bankCard.lastError.length > 0 && bankCard.st === 2
                                text: bankCard.lastError + qsTr(" — try again in a moment.")
                                color: Theme.warning
                                font.pixelSize: Theme.fontSizeCaption
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    GridLayout {
                        columns: FormFactor.compact ? 2 : 4
                        columnSpacing: Theme.spacingS
                        rowSpacing: Theme.spacingXS
                        Layout.alignment: (FormFactor.compact ? Qt.AlignLeft : Qt.AlignRight)
                                          | Qt.AlignVCenter

                        Button {
                            visible: bankCard.st === 0 || bankCard.st === 3 // Disconnected / NeedsReauth
                            highlighted: true
                            text: bankCard.st === 3 ? qsTr("Reconnect") : qsTr("Connect")
                            onClicked: connectFlow.openForBank(bankCard.index, 1) // straight to credentials
                        }

                        Button {
                            visible: bankCard.st === 2 || bankCard.st === 4
                            text: bankCard.syncing ? qsTr("…") : qsTr("Refresh")
                            enabled: !bankCard.syncing
                            onClicked: {
                                if (!root.bank.connections.refresh_bank(bankCard.index))
                                    root.showTransient(qsTr("Cannot sync right now — reconnect required."))
                            }
                        }

                        Button {
                            visible: bankCard.st !== 0
                            flat: true
                            text: qsTr("Disconnect")
                            onClicked: disconnectConfirm.openFor(bankCard.index)
                        }

                        Button {
                            visible: bankCard.isBt && bankCard.st === 2
                            flat: true
                            text: qsTr("Demo: expire session")
                            font.pixelSize: Theme.fontSizeCaption
                            onClicked: root.bank.force_bt_session_expiry()
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Linked banks sync automatically on staggered schedules.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeCaption
        }
    }

    function showTransient(message) {
        transientText.text = message
        transientTimer.restart()
    }

    Rectangle {
        Layout.fillWidth: true
        visible: transientText.text.length > 0
        implicitHeight: transientText.implicitHeight + 2 * Theme.spacingM
        radius: Theme.radiusM
        color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b,
                       Theme.isDark ? 0.18 : 0.12)
        border.color: Theme.danger

        Label {
            id: transientText
            anchors.fill: parent
            anchors.margins: Theme.spacingM
            color: Theme.text
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }

    Timer { id: transientTimer; interval: 4000; onTriggered: transientText.text = "" }

    ConnectBankFlow { id: connectFlow; bank: root.bank }

    Dialog {
        id: disconnectConfirm

        property int targetBank: -1

        function openFor(bankIdx) { targetBank = bankIdx; open() }

        anchors.centerIn: parent
        modal: true
        title: qsTr("Disconnect %1?").arg(Theme.banks[targetBank >= 0 ? targetBank : 0].name)
        standardButtons: Dialog.Cancel | Dialog.Yes

        Label {
            width: parent.width
            text: qsTr("The aggregator removes all data received from this bank.")
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeSmall
        }

        onAccepted: root.bank.connections.disconnect_bank(targetBank)
    }
}
