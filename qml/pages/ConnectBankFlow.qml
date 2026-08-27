import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Onboarding wizard (plan §3): picker -> mock credentials -> consent scopes.
// Authorize hands over to ConnectionService; skeletons live on the page.
Dialog {
    id: root

    required property var bank
    readonly property var _taglines: [
        qsTr("Your primary bank"),
        qsTr("Incumbent, rich history"),
        qsTr("Challenger, best FX rates"),
        qsTr("EUR desk specialist")
    ]
    readonly property var _scopes: [
        qsTr("Read account balances"),
        qsTr("Read last 90 days of transactions"),
        qsTr("Request FX quotes at your banks")
    ]

    property int _step: 0
    property int _chosenBank: -1

    // Guarded: _chosenBank is -1 until a bank is picked, and the title binding
    // re-evaluates on every _step change.
    readonly property string _chosenBankName:
        _chosenBank >= 0 && _chosenBank < Theme.banks.length
            ? Theme.banks[_chosenBank].name : ""

    function openFlow() {
        _step = 0
        _chosenBank = -1
        credUser.text = "demo"
        credPass.text = "demo-pass"
        open()
    }

    function openForBank(bankIndex, step) {
        _chosenBank = bankIndex
        _step = Math.max(0, step)
        credUser.text = "demo"
        credPass.text = "demo-pass"
        open()
    }

    anchors.centerIn: parent
    modal: true
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 48 : 520)
    title: _step === 0 ? qsTr("Connect a bank")
         : _step === 1 ? qsTr("Sign in to %1").arg(_chosenBankName)
                       : qsTr("Consent — %1").arg(_chosenBankName)

    footer: DialogButtonBox {
        Button {
            flat: true
            text: qsTr("Back")
            visible: root._step > 0
            onClicked: root._step--
        }
        Button {
            highlighted: root._step === 2
            text: root._step === 0 ? qsTr("Next") : root._step === 1 ? qsTr("Sign in") : qsTr("Authorize")
            enabled: root._step === 0 ? root._chosenBank >= 0 : true
            onClicked: {
                if (root._step < 2) {
                    root._step++
                } else {
                    root.bank.connections.link_bank(root._chosenBank,
                                                   credUser.text, credPass.text, "")
                    root.accept()
                }
            }
        }
        Button {
            flat: true
            text: qsTr("Cancel")
            onClicked: root.reject()
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingM

        StackLayout {
            Layout.fillWidth: true
            currentIndex: root._step

            // ---- step 0: bank picker -----------------------------------
            GridView {
                id: pickerGrid
                implicitHeight: 260
                interactive: false
                cellWidth: width / 2
                cellHeight: 120
                model: 4

                delegate: ItemDelegate {
                    id: pick
                    required property int index
                    width: pickerGrid.cellWidth - 6
                    height: pickerGrid.cellHeight - 6
                    x: 3; y: 3

                    readonly property bool linked:
                        root.bank.connections.state(pick.index) !== 0 // 0 = Disconnected

                    background: Rectangle {
                        radius: Theme.radiusM
                        color: pick.hovered ? Theme.surfaceAlt : Theme.surface
                        border.color: pick.hovered ? Theme.accent : Theme.border
                    }

                    contentItem: ColumnLayout {
                        spacing: Theme.spacingXS

                        RowLayout {
                            spacing: Theme.spacingS
                            BankBadge { bank_id: pick.index; size: 28 }
                            Label {
                                text: Theme.banks[pick.index].name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            text: root._taglines[pick.index]
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: pick.linked
                            text: qsTr("Already connected")
                            color: Theme.success
                            font.pixelSize: Theme.fontSizeCaption
                        }
                        Item { Layout.fillHeight: true }
                    }

                    onClicked: {
                        if (!pick.linked) {
                            root._chosenBank = pick.index
                            root._step = 1
                        }
                    }
                }
            }

            // ---- step 1: mock credentials -------------------------------
            ColumnLayout {
                spacing: Theme.spacingM

                Label {
                    text: qsTr("This demo accepts anything non-empty.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                }
                TextField {
                    id: credUser
                    Layout.fillWidth: true
                    placeholderText: qsTr("Username")
                }
                TextField {
                    id: credPass
                    Layout.fillWidth: true
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                }
                Item { Layout.fillHeight: true }
            }

            // ---- step 2: consent ----------------------------------------
            ColumnLayout {
                spacing: Theme.spacingS

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 will get read-only access to:",
                               "").arg(root._chosenBankName)
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root._scopes
                    delegate: RowLayout {
                        id: scopeRow
                        required property string modelData
                        spacing: Theme.spacingS
                        Image {
                            source: Theme.icon("plug")
                            sourceSize: Qt.size(16, 16)
                        }
                        Label {
                            text: scopeRow.modelData
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("You can disconnect at any time from this screen.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
