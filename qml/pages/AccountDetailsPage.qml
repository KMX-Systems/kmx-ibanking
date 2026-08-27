import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Per-account statement view (plan §Phase 4): balance, monthly in/out,
// IBAN share, transaction history. Pushed onto the shell StackView.
Page {
    id: root

    required property var bank
    property int account_id: -1

    signal backRequested()

    background: null

    Component.onCompleted: {
        bank.statement_transactions.account_filter = account_id
        // Page.title is what the mobile top bar reads back (RouteHost.currentTitle).
        root.title = bank.account_name(account_id)
    }

    header: RowLayout {
        id: headerBar

        spacing: Theme.spacingS

        // The mobile shell already carries a back affordance and the title.
        ToolButton {
            Layout.leftMargin: FormFactor.pageMargin
            visible: !FormFactor.compact
            icon.source: Theme.icon("chevleft")
            onClicked: root.backRequested()
            Accessible.name: qsTr("Back")
        }

        Label {
            visible: !FormFactor.compact
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontSizeH2
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Item { visible: FormFactor.compact; Layout.fillWidth: true }

        ToolButton {
            Layout.rightMargin: FormFactor.pageMargin
            icon.source: Theme.icon("receipt")
            Accessible.name: qsTr("Copy IBAN")
            onClicked: {
                root.bank.copy_to_clipboard(root.bank.account_iban(root.account_id))
                root.bank.notifications.post("success", qsTr("IBAN copied"))
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        spacing: Theme.spacingL

        // ---- summary cards ---------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: FormFactor.compact ? 2 : 3
            columnSpacing: Theme.spacingS
            rowSpacing: Theme.spacingS

            component SummaryCard : Frame {
                id: card

                property string caption
                property int amount_minor
                property color amountColor: Theme.text
                property bool signedAmount: false

                Layout.fillWidth: true

                background: Rectangle {
                    radius: Theme.radiusM
                    color: Theme.surface
                    border.color: Theme.border
                }

                contentItem: ColumnLayout {
                    spacing: 2
                    Label {
                        text: card.caption
                        color: card.amountColor === Theme.text ? Theme.textMuted
                                                               : card.amountColor
                        font.pixelSize: Theme.fontSizeCaption
                    }
                    CurrencyLabel {
                        minor: card.amount_minor
                        currency_code: root.bank.account_currency_code(root.account_id)
                        signedDisplay: card.signedAmount
                        tone: "neutral"
                        pixelSize: Theme.fontSizeH3
                    }
                }
            }

            SummaryCard {
                caption: qsTr("Balance")
                amount_minor: root.bank.account_balance(root.account_id)
                Layout.columnSpan: FormFactor.compact ? 2 : 1
            }

            SummaryCard {
                caption: qsTr("In this month")
                amount_minor: root.bank.month_in_out(root.account_id)[0]
                amountColor: Theme.success
                signedAmount: true
            }

            SummaryCard {
                caption: qsTr("Out this month")
                amount_minor: -root.bank.month_in_out(root.account_id)[1]
                amountColor: Theme.danger
                signedAmount: true
            }
        }

        Label {
            text: root.bank.account_iban(root.account_id)
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        SectionHeader { title: qsTr("Statement") }

        ListView {
            id: statementList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacingXS
            model: root.bank.statement_transactions

            delegate: RowLayout {
                width: statementList.width
                height: 46
                spacing: Theme.spacingS

                CategoryChip { category: model.category }

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true

                    Label {
                        text: model.counterparty
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Qt.formatDateTime(model.posted_at, "dd MMM yyyy · hh:mm")
                              + (model.status === 0 ? qsTr(" · pending") : "")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }

                CurrencyLabel {
                    minor: model.signed_amount_minor
                    currency_code: model.currency_code
                    signedDisplay: true
                    pixelSize: Theme.fontSizeSmall
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
