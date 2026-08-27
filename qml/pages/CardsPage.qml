import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Card management (plan §Phase 8): 3D flip reveal, freeze animation,
// limits, toggles, virtual card creation gated per bank.
Page {
    id: root

    required property var bank

    background: null

    title: qsTr("Cards")

    function _networkName(net) { return net === 0 ? "VISA" : "Mastercard" }
    function _brandColor(net) { return net === 0 ? "#1a1f71" : "#eb001b" }

    header: RowLayout {
        spacing: Theme.spacingS

        SectionHeader {
            title: root.title
            pageHeader: true
            reserveSpace: true
            Layout.fillWidth: true
        }

        Button {
            Layout.rightMargin: FormFactor.pageMargin
            flat: true
            icon.source: Theme.icon("plus")
            text: qsTr("Virtual card")
            enabled: virtualAccounts.count > 0
            onClicked: virtualDialog.open()
        }
    }

    ListModel { id: virtualAccounts }

    function refreshVirtualAccounts() {
        const m = root.bank ? root.bank.account_model : null
        if (!m)
            return
        virtualAccounts.clear()
        for (let i = 0; i < m.rowCount(); ++i) {
            const row = m.account_row_at(i)
            if (row.row_type === undefined || row.row_type !== 1)
                continue
            if (!root.bank.connections.supports_virtual_cards(row.bank_id))
                continue
            virtualAccounts.append({
                account_id: row.account_id,
                label: row.name + " · " + row.currency_code
            })
        }
        accountCombo.currentIndex = virtualAccounts.count > 0 ? 0 : -1
    }

    Component.onCompleted: Qt.callLater(refreshVirtualAccounts)
    Connections {
        target: root.bank.account_model
        function onModel_rebuilt() { root.refreshVirtualAccounts() }
    }

    ScrollView {
        id: scroller
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroller.availableWidth
            spacing: FormFactor.cardSpacing

            Repeater {
                model: root.bank.cards

                delegate: ColumnLayout {
                    id: cardBlock
                    required property int index
                    required property var model

                    spacing: Theme.spacingS
                    Layout.fillWidth: true

                    // ---- the card (tap to flip) --------------------------
                    Item {
                        id: flipper
                        property real angle: 0

                        Layout.fillWidth: true
                        Layout.preferredHeight: 190

                        transform: Rotation {
                            origin.x: flipper.width / 2
                            origin.y: flipper.height / 2
                            axis { x: 1; y: 0; z: 0 }
                            angle: flipper.angle
                        }

                        Behavior on angle {
                            NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                        }

                        Rectangle {
                            id: frontFace
                            anchors.fill: parent
                            radius: Theme.radiusL
                            visible: flipper.angle < 90
                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0; color: root._brandColor(cardBlock.model.network) }
                                GradientStop { position: 1; color: Qt.darker(root._brandColor(cardBlock.model.network), 1.6) }
                            }
                            border.color: Qt.lighter(root._brandColor(cardBlock.model.network), 1.4)

                            opacity: 1 - Math.min(1, flipper.angle / 90)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingL

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: cardBlock.model.label
                                              + (cardBlock.model.is_virtual ? qsTr("  · virtual") : "")
                                        color: "#e8ecf2"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: root._networkName(cardBlock.model.network)
                                        color: "#cfd6df"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                        font.letterSpacing: 2
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                Label {
                                    text: cardBlock.model.frozen ? qsTr("❄ FROZEN")
                                                                 : cardBlock.model.masked_pan
                                    color: cardBlock.model.frozen ? "#9fc3ef" : "#f0f3f7"
                                    font: {
                                        const f = Theme.amountFont(Theme.fontSizeH3)
                                        f.letterSpacing = cardBlock.model.frozen ? 0 : 2
                                        return f
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: cardBlock.model.holder
                                        color: "#cfd6df"
                                        font.pixelSize: Theme.fontSizeCaption
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: cardBlock.model.expiry
                                        color: "#cfd6df"
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                }
                            }

                            TapHandler { onTapped: flipper.angle = 180 }
                        }

                        Rectangle {
                            id: backFace
                            anchors.fill: parent
                            radius: Theme.radiusL
                            visible: flipper.angle >= 90
                            color: "#20262e"
                            border.color: Theme.border

                            opacity: Math.min(1, (flipper.angle - 90) / 90)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingL
                                spacing: Theme.spacingXS

                                Label {
                                    text: qsTr("Card details")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }

                                RowLayout {
                                    spacing: Theme.spacingM
                                    Label {
                                        text: cardBlock.model.full_pan
                                        color: "#f0f3f7"
                                        font: {
                                            const f = Theme.amountFont(Theme.fontSizeH3)
                                            f.letterSpacing = 2
                                            return f
                                        }
                                    }
                                    Label {
                                        text: qsTr("CVV %1").arg(cardBlock.model.cvv)
                                        color: "#9fb3c8"
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }

                                ToolButton {
                                    icon.source: Theme.icon("receipt")
                                    Accessible.name: qsTr("Copy card number")
                                    onClicked: {
                                        root.bank.copy_to_clipboard(cardBlock.model.full_pan)
                                        root.bank.notifications.post("success",
                                                                     qsTr("Card number copied"))
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                Label {
                                    text: cardBlock.model.holder + " · " + cardBlock.model.expiry
                                          + (cardBlock.model.is_virtual ? qsTr(" · virtual") : "")
                                    color: "#9fb3c8"
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            TapHandler { onTapped: flipper.angle = 0 }
                        }
                    }

                    // ---- controls ----------------------------------------
                    Frame {
                        Layout.fillWidth: true
                        background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                                border.color: Theme.border }
                        contentItem: ColumnLayout {
                            spacing: Theme.spacingS

                            // Three labelled switches in one row need ~420px;
                            // below that they stack as label/control pairs.
                            GridLayout {
                                Layout.fillWidth: true
                                columns: FormFactor.compact ? 2 : 7
                                columnSpacing: Theme.spacingM
                                rowSpacing: Theme.spacingXS

                                Label {
                                    text: qsTr("Freeze")
                                    color: cardBlock.model.frozen ? Theme.danger : Theme.text
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                Switch {
                                    checked: cardBlock.model.frozen
                                    Accessible.name: qsTr("Freeze card")
                                    onToggled: function (on) {
                                        if (root.bank.set_card_frozen(cardBlock.model.card_id, on))
                                            root.bank.notifications.post(
                                                on ? "warning" : "success",
                                                on ? qsTr("Card frozen") : qsTr("Card unfrozen"),
                                                cardBlock.model.label)
                                    }
                                }

                                Item { visible: !FormFactor.compact; Layout.fillWidth: true }

                                Label {
                                    text: qsTr("Online")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                                Switch {
                                    checked: cardBlock.model.online_payments
                                    onToggled: (on) => root.bank.set_card_online(
                                                   cardBlock.model.card_id, on)
                                }

                                Label {
                                    text: qsTr("Tap&Go")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                                Switch {
                                    checked: cardBlock.model.contactless
                                    onToggled: (on) => root.bank.set_card_contactless(
                                                   cardBlock.model.card_id, on)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingM

                                Label {
                                    text: qsTr("Daily limit")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }

                                Slider {
                                    id: limitSlider
                                    from: 0
                                    to: 500000
                                    stepSize: 50
                                    value: cardBlock.model.daily_limit_minor / 100
                                    Layout.fillWidth: true

                                    onMoved: limitTimer.restart()

                                    Timer {
                                        id: limitTimer
                                        interval: 350
                                        onTriggered: {
                                            const minor = Math.round(limitSlider.value * 100)
                                            const res = root.bank.set_card_limit(
                                                cardBlock.model.card_id, minor)
                                            if (res.ok !== true)
                                                root.bank.notifications.post("warning",
                                                                             res.message.toString())
                                        }
                                    }
                                }

                                CurrencyLabel {
                                    minor: limitSlider.value * 100
                                    currency_code: "RON"
                                    tone: "neutral"
                                    pixelSize: Theme.fontSizeCaption
                                }
                            }
                        }
                    }
                }
            }

            EmptyState {
                visible: root.bank.cards.rowCount() === 0
                iconSource: Theme.icon("card")
                title: qsTr("No cards linked yet")
                message: qsTr("Cards appear here once your banks sync them.")
            }

            Item { height: Theme.spacingS }
        }
    }

    Dialog {
        id: virtualDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Create a virtual card")

        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 48 : 420)

        onAboutToShow: root.refreshVirtualAccounts()

        onAccepted: {
            if (accountCombo.currentIndex < 0)
                return
            const entry = virtualAccounts.get(accountCombo.currentIndex)
            const res = root.bank.create_virtual_card(entry.account_id,
                                                    labelField.text ||
                                                        qsTr("Virtual card"),
                                                    Math.round(limitSpin.value * 100))
            if (res.ok === true)
                root.bank.notifications.post("success", qsTr("Virtual card created"))
            else
                root.bank.notifications.post("warning", res.message.toString())
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacingM

            ComboBox {
                id: accountCombo
                Layout.fillWidth: true
                textRole: "label"
                model: virtualAccounts
            }

            TextField {
                id: labelField
                Layout.fillWidth: true
                placeholderText: qsTr("Card label (optional)")
                font.pixelSize: Theme.fontSizeSmall
            }

            RowLayout {
                spacing: Theme.spacingM
                Label { text: qsTr("Daily limit"); color: Theme.textMuted;
                        font.pixelSize: Theme.fontSizeSmall }
                SpinBox {
                    id: limitSpin
                    from: 100; to: 200000
                    value: 20000
                    editable: true
                }
                CurrencyLabel { minor: limitSpin.value * 100; currency_code: "RON";
                                tone: "neutral"; pixelSize: Theme.fontSizeCaption }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Only banks that issue virtual cards are listed.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.WordWrap
            }
        }
    }
}
