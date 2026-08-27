import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// All accounts grouped by bank with collapsible headers (plan §Phase 4).
Page {
    id: root

    required property var bank

    signal accountSelected(int account_id)

    background: null

    title: qsTr("Accounts")

    header: SectionHeader { title: root.title; pageHeader: true }

    ListView {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        clip: true
        spacing: Theme.spacingXS
        model: root.bank.account_model

        delegate: Loader {
            required property int index
            required property var model

            width: ListView.view.width
            sourceComponent: model.row_type === 0 ? headerComp : rowComp

            Component {
                id: headerComp

                ItemDelegate {
                    height: 42
                    width: parent ? parent.width : 0

                    background: Rectangle {
                        radius: Theme.radiusS
                        color: Theme.surfaceAlt
                    }

                    contentItem: RowLayout {
                        spacing: Theme.spacingS

                        BankBadge { bank_id: model.bank_id; size: 22 }

                        Label {
                            text: model.name
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        CurrencyLabel {
                            minor: model.display_balance_minor
                            currency_code: Theme.currency_code(root.bank.display_currency)
                            tone: "neutral"
                            pixelSize: Theme.fontSizeSmall
                        }

                        Image {
                            source: Theme.icon(model.collapsed ? "chevright" : "chevleft")
                            sourceSize: Qt.size(14, 14)
                        }
                    }

                    onClicked: root.bank.account_model.toggle_collapsed(model.bank_id)
                }
            }

            Component {
                id: rowComp

                Frame {
                    leftPadding: Theme.spacingL

                    background: Rectangle {
                        radius: Theme.radiusM
                        color: Theme.surface
                        border.color: Theme.border
                    }

                    contentItem: ColumnLayout {
                        spacing: Theme.spacingXS

                        RowLayout {
                            spacing: Theme.spacingS

                            Label {
                                text: model.name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeBody
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            CurrencyLabel {
                                minor: model.balance_minor
                                currency_code: model.currency_code
                                pixelSize: Theme.fontSizeBody
                            }
                        }

                        RowLayout {
                            spacing: Theme.spacingS

                            Rectangle {
                                radius: Theme.radiusS
                                height: 18
                                width: kindLabel.implicitWidth + 12
                                color: model.kind === 2 ? Qt.alpha(Theme.danger, 0.15)
                                      : model.kind === 1 ? Qt.alpha(Theme.success, 0.15)
                                      : Qt.alpha(Theme.info, 0.15)

                                Label {
                                    id: kindLabel
                                    anchors.centerIn: parent
                                    text: model.kind === 2 ? qsTr("Credit")
                                         : model.kind === 1 ? qsTr("Savings")
                                         : qsTr("Checking")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            Label {
                                text: model.masked_iban
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeCaption
                                Layout.fillWidth: true
                            }
                        }
                    }

                    TapHandler { onTapped: root.accountSelected(model.account_id) }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {}
    }
}
