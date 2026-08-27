import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank
import "../theme/Format.js" as Format

// Cross-bank dashboard (plan §Phase 4): net-worth hero, per-bank subtotals,
// quick actions, account cards, budget snapshot, cross-bank activity feed.
Page {
    id: root

    required property var bank

    signal newTransferRequested()
    signal exchangeRequested()
    signal freezeCardRequested()
    signal seeAllAccountsRequested()
    signal accountSelected(int account_id)

    background: null

    readonly property string _dispCode: Theme.currency_code(bank.display_currency)

    // Three labelled actions side by side only fit once the label sits under
    // the icon rather than beside it.
    readonly property int _actionDisplay: FormFactor.compact ? AbstractButton.TextUnderIcon
                                                             : AbstractButton.TextBesideIcon

    title: qsTr("Dashboard")

    header: SectionHeader { title: root.title; pageHeader: true }

    ScrollView {
        id: scroller
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroller.availableWidth
            spacing: FormFactor.cardSpacing

            // ---- net-worth hero -----------------------------------------
            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    radius: Theme.radiusL
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.isDark ? "#1a3a5c" : "#1d3d5e" }
                        GradientStop { position: 1; color: Theme.isDark ? "#0f2237" : "#16324f" }
                    }
                }

                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Total across %1 banks")
                                  .arg(root.bank.bank_subtotals.length)
                            color: "#bcd3ea"
                            font.pixelSize: Theme.fontSizeSmall
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: currencyBox
                            model: Theme.currencies
                            currentIndex: root.bank.display_currency
                            font.pixelSize: Theme.fontSizeCaption
                            onActivated: function (index) {
                                root.bank.display_currency = index
                                currentIndex = root.bank.display_currency
                            }
                        }
                    }

                    AnimatedBalance {
                        minor: root.bank.net_worth_minor
                        currency_code: root._dispCode
                        pixelSize: Theme.fontSizeH1 + 6
                        valueColor: "white"
                    }

                    Label {
                        text: qsTr("Indicative mid-market value · updates live")
                        color: "#8fb0cf"
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }
            }

            // ---- per-bank subtotal chips ---------------------------------
            // Four abreast needs ~90px each; wrap to a grid on a handset.
            GridLayout {
                Layout.fillWidth: true
                columns: Math.min(FormFactor.tileColumns, 4)
                columnSpacing: Theme.spacingS
                rowSpacing: Theme.spacingS

                Repeater {
                    model: root.bank.bank_subtotals

                    delegate: Frame {
                        id: chip
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 60

                        background: Rectangle {
                            radius: Theme.radiusM
                            color: Theme.surface
                            border.color: Theme.border
                        }

                        contentItem: RowLayout {
                            spacing: Theme.spacingS

                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: Theme.bankColor(chip.modelData.bank_id)
                            }
                            Label {
                                text: Theme.banks[chip.modelData.bank_id].name.split(" ")[0]
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeCaption
                            }
                            CurrencyLabel {
                                minor: chip.modelData.total_minor
                                currency_code: root._dispCode
                                tone: "neutral"
                                pixelSize: Theme.fontSizeSmall
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // ---- exchange insight cards (plan §6.3, threshold-gated) ------
            Repeater {
                model: root.bank.exchange_insights

                delegate: Frame {
                    id: insightCard
                    required property var modelData

                    Layout.fillWidth: true

                    background: Rectangle {
                        radius: Theme.radiusM
                        color: Qt.alpha(Theme.info, Theme.isDark ? 0.16 : 0.10)
                        border.color: Theme.info
                    }

                    contentItem: ColumnLayout {
                        spacing: Theme.spacingXS

                        RowLayout {
                            spacing: Theme.spacingS
                            Image {
                                source: Theme.icon("trend")
                                sourceSize: Qt.size(18, 18)
                            }
                            Label {
                                text: qsTr("Smart exchange insight")
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            BankBadge { bank_id: insightCard.modelData.bank_id; size: 20 }
                        }

                        Label {
                            text: qsTr("Your %1 (%2) could convert ~%3 cheaper at %4.")
                                  .arg(insightCard.modelData.account_name)
                                  .arg(insightCard.modelData.from_code)
                                  .arg(Format.money(insightCard.modelData.savings_minor,
                                                    insightCard.modelData.to_code))
                                  .arg(Theme.banks[insightCard.modelData.recommended_bank].name.split(" ")[0])
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    TapHandler {
                        onTapped: root.exchangeRequested()
                    }
                }
            }

            // ---- quick actions --------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingS

                Button {
                    Layout.fillWidth: true
                    display: root._actionDisplay
                    icon.source: Theme.icon("swap")
                    text: qsTr("New transfer")
                    onClicked: root.newTransferRequested()
                }
                Button {
                    Layout.fillWidth: true
                    display: root._actionDisplay
                    icon.source: Theme.icon("trend")
                    text: qsTr("Exchange")
                    onClicked: root.exchangeRequested()
                }
                Button {
                    Layout.fillWidth: true
                    display: root._actionDisplay
                    icon.source: Theme.icon("lock")
                    text: qsTr("Freeze a card")
                    onClicked: root.freezeCardRequested()
                }
            }

            // ---- accounts ---------------------------------------------------
            SectionHeader {
                title: qsTr("Accounts")
                actionText: qsTr("See all")
                onActionTriggered: root.seeAllAccountsRequested()
            }

            Repeater {
                model: root.bank.account_model

                delegate: Loader {
                    required property int index
                    required property var model

                    Layout.fillWidth: true
                    sourceComponent: model.row_type === 0 ? emptyComp : accountCard

                    Component { id: emptyComp; Item { height: 0 } }

                    Component {
                        id: accountCard

                        Frame {
                            Layout.fillWidth: true

                            background: Rectangle {
                                radius: Theme.radiusM
                                color: Theme.surface
                                border.color: Theme.border
                            }

                            contentItem: ColumnLayout {
                                spacing: Theme.spacingXS

                                RowLayout {
                                    spacing: Theme.spacingS

                                    BankBadge { bank_id: model.bank_id; size: 26 }

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
                                        pixelSize: Theme.fontSizeH3
                                    }
                                }

                                RowLayout {
                                    spacing: Theme.spacingS

                                    Label {
                                        text: model.masked_iban
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeCaption
                                        Layout.fillWidth: true
                                    }

                                    ToolButton {
                                        icon.source: Theme.icon("receipt")
                                        icon.width: 16
                                        icon.height: 16
                                        Accessible.name: qsTr("Copy IBAN")
                                        onClicked: {
                                            root.bank.copy_to_clipboard(model.iban)
                                            root.bank.notifications.post(
                                                        "success", qsTr("IBAN copied"),
                                                        model.name)
                                        }
                                    }

                                    Sparkline {
                                        visible: !FormFactor.compact
                                        values: model.sparkline
                                        width: visible ? 90 : 0
                                        height: 26
                                        lineColor: {
                                            const v = model.sparkline
                                            if (!v || v.length < 2)
                                                return Theme.textMuted
                                            return v[v.length - 1] >= v[0]
                                                    ? Theme.success : Theme.danger
                                        }
                                    }
                                }
                            }

                            TapHandler {
                                onTapped: root.accountSelected(model.account_id)
                            }
                        }
                    }
                }
            }

            // ---- budgets mini-widget ----------------------------------------
            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    radius: Theme.radiusM
                    color: Theme.surface
                    border.color: Theme.border
                }

                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    Label {
                        text: qsTr("This month's budgets")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                    }

                    Repeater {
                        model: root.bank.budget_progress

                        delegate: RowLayout {
                            id: budgetRow
                            required property var modelData
                            spacing: Theme.spacingS
                            Layout.fillWidth: true

                            CategoryChip { category: budgetRow.modelData.category }

                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: Math.max(1, budgetRow.modelData.limit_minor)
                                value: Math.min(budgetRow.modelData.spent_minor,
                                                budgetRow.modelData.limit_minor * 1.05)
                            }

                            CurrencyLabel {
                                visible: !FormFactor.compact
                                minor: budgetRow.modelData.spent_minor
                                currency_code: "RON"
                                tone: "neutral"
                                pixelSize: Theme.fontSizeCaption
                            }

                            Label {
                                text: Format.percent(
                                          Math.min(1, budgetRow.modelData.spent_minor /
                                                   Math.max(1, budgetRow.modelData.limit_minor)))
                                color: budgetRow.modelData.spent_minor >
                                       budgetRow.modelData.limit_minor
                                       ? Theme.danger : Theme.textMuted
                                font.pixelSize: Theme.fontSizeCaption
                                Layout.preferredWidth: 44
                            }
                        }
                    }
                }
            }

            // ---- recent activity across banks -------------------------------
            SectionHeader { title: qsTr("Recent activity") }

            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    radius: Theme.radiusM
                    color: Theme.surface
                    border.color: Theme.border
                }

                contentItem: ColumnLayout {
                    spacing: 0

                    Repeater {
                        model: root.bank.recent_transactions

                        delegate: RowLayout {
                            id: txnRow
                            required property int index
                            required property string counterparty
                            required property int signed_amount_minor
                            required property string currency_code
                            required property var posted_at
                            required property int category
                            required property int bank_id

                            spacing: Theme.spacingS
                            Layout.fillWidth: true
                            Layout.topMargin: index === 0 ? 0 : Theme.spacingXS

                            BankBadge { bank_id: txnRow.bank_id; size: 24 }

                            CategoryChip { category: txnRow.category }

                            Label {
                                text: txnRow.counterparty
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Five columns do not fit a handset; the date is
                            // the least useful one in a "recent" list.
                            Label {
                                visible: !FormFactor.compact
                                text: Qt.formatDate(txnRow.posted_at, "dd MMM")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeCaption
                            }

                            CurrencyLabel {
                                minor: txnRow.signed_amount_minor
                                currency_code: txnRow.currency_code
                                signedDisplay: true
                                pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }
                }
            }

            Item { height: Theme.spacingS }
        }
    }
}
