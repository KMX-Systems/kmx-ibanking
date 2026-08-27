import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank
import "../theme/Format.js" as Format

// Analytics & budgets (plan §Phase 9): spending donut, monthly cashflow,
// net-worth trend, budget envelopes with editing, FX desk overview.
// Donut reconciles with the ledger by construction (single source: AnalyticsService).
Page {
    id: root

    required property var bank

    background: null

    title: qsTr("Analytics")

    property int viewYear: new Date().getFullYear()
    property int viewMonth: new Date().getMonth() + 1
    readonly property string monthTitle:
        Format.monthName(viewYear, viewMonth)
    // One computation shared by donut, legend and empty-state.
    readonly property var breakdown:
        bank.analytics.category_breakdown(viewYear, viewMonth)

    function shiftMonth(delta) {
        let m = viewMonth + delta
        let y = viewYear
        if (m < 1) { m = 12; y-- }
        if (m > 12) { m = 1; y++ }
        viewMonth = m
        viewYear = y
    }

    header: RowLayout {
        spacing: Theme.spacingS

        ToolButton {
            Layout.leftMargin: FormFactor.pageMargin
            icon.source: Theme.icon("chevleft")
            onClicked: root.shiftMonth(-1)
            Accessible.name: qsTr("Previous month")
        }
        Label {
            text: root.monthTitle + " " + root.viewYear
            color: Theme.text
            font.pixelSize: Theme.fontSizeH2
            font.bold: true
        }
        ToolButton {
            icon.source: Theme.icon("chevright")
            enabled: !(root.viewYear === new Date().getFullYear() &&
                       root.viewMonth === new Date().getMonth() + 1)
            onClicked: root.shiftMonth(1)
            Accessible.name: qsTr("Next month")
        }
        Item { Layout.fillWidth: true }
        SectionHeader { title: root.title; pageHeader: true; reserveSpace: true; Layout.fillWidth: true }
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

            // ---- donut + legend -------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }

                contentItem: GridLayout {
                    columns: FormFactor.compact ? 1 : 2
                    columnSpacing: Theme.spacingL
                    rowSpacing: Theme.spacingM

                    ColumnLayout {
                        spacing: Theme.spacingS
                        Label {
                            text: qsTr("Spending by category · %1").arg(root.monthTitle)
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        DonutChart {
                            id: donut
                            segments: root.breakdown.map(function (r) {
                                return { category: r.category, spent_minor: r.spent_minor }
                            })
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 200
                        }
                    }

                    ColumnLayout {
                        spacing: Theme.spacingXS
                        Layout.fillWidth: true

                        Repeater {
                            model: root.breakdown

                            delegate: RowLayout {
                                required property int index
                                required property var modelData
                                spacing: Theme.spacingS
                                Layout.fillWidth: true

                                CategoryChip { category: modelData.category }

                                CurrencyLabel {
                                    minor: modelData.spent_minor
                                    currency_code: "RON"
                                    tone: "neutral"
                                    pixelSize: Theme.fontSizeCaption
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        Label {
                            visible: root.breakdown.length === 0
                            text: qsTr("No spending recorded this month.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }
                }
            }

            // ---- cashflow bars ----------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS
                    RowLayout {
                        spacing: Theme.spacingM
                        Label {
                            text: qsTr("Income vs expenses")
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Rectangle { width: 10; height: 10; radius: 2; color: Theme.success }
                        Label { text: qsTr("in"); color: Theme.textMuted;
                                font.pixelSize: Theme.fontSizeCaption }
                        Rectangle { width: 10; height: 10; radius: 2; color: Theme.danger }
                        Label { text: qsTr("out"); color: Theme.textMuted;
                                font.pixelSize: Theme.fontSizeCaption }
                    }
                    CashflowBars {
                        Layout.fillWidth: true
                        implicitHeight: 170
                        rows: root.bank.analytics.month_cashflow(6)
                    }
                }
            }

            // ---- net worth trend ---------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS
                    Label {
                        text: qsTr("Net-worth trend (RON)")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    TrendChart {
                        Layout.fillWidth: true
                        implicitHeight: 170
                        rows: root.bank.analytics.net_worth_series(7)
                    }
                }
            }

            // ---- budget envelopes ---------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    Label {
                        text: qsTr("Budgets · %1").arg(root.monthTitle)
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Repeater {
                        model: root.bank.budgets.budgets_for_month(root.viewYear,
                                                                 root.viewMonth)

                        delegate: RowLayout {
                            id: envRow
                            required property int index
                            required property var modelData

                            readonly property bool over:
                                modelData.spent_minor > modelData.limit_minor

                            spacing: Theme.spacingS
                            Layout.fillWidth: true

                            CategoryChip { category: envRow.modelData.category }

                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: Math.max(1, envRow.modelData.limit_minor)
                                value: Math.min(envRow.modelData.spent_minor, to * 1.05)
                            }

                            Label {
                                text: Format.money(envRow.modelData.spent_minor, "RON")
                                      + " / " + Format.money(envRow.modelData.limit_minor, "RON")
                                color: envRow.over ? Theme.danger : Theme.textMuted
                                font.pixelSize: Theme.fontSizeCaption
                            }

                            ToolButton {
                                icon.source: Theme.icon("sliders")
                                Accessible.name: qsTr("Edit limit")
                                onClicked: limitDialog.openFor(envRow.modelData.category,
                                                               envRow.modelData.limit_minor)
                            }
                        }
                    }
                }
            }

            // ---- FX desk overview -----------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    Label {
                        text: qsTr("FX desks")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    GridLayout {
                        columns: FormFactor.compact ? 1 : 3
                        columnSpacing: Theme.spacingM
                        rowSpacing: Theme.spacingS
                        Layout.fillWidth: true

                        Repeater {
                            model: [
                                { from: 0, to: 1, caption: "RON → EUR" },
                                { from: 0, to: 2, caption: "RON → USD" },
                                { from: 1, to: 0, caption: "EUR → RON" }
                            ]
                            delegate: ColumnLayout {
                                required property var modelData
                                spacing: 2

                                Label {
                                    text: modelData.caption
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                                Label {
                                    text: Number(root.bank.advisor_mid_rate(
                                                     modelData.from,
                                                     modelData.to)).toFixed(4)
                                    color: Theme.accent
                                    font: Theme.amountFont(Theme.fontSizeBody)
                                }
                                Sparkline {
                                    values: root.bank.rate_history(modelData.from,
                                                                  modelData.to)
                                    width: 110
                                    height: 26
                                    lineColor: Theme.accent
                                }
                            }
                        }
                    }

                    Label {
                        text: qsTr("Desk spreads: KMX 0.15% · TBI 0.08% · BT 0.55%+fee · Erste 0.20/0.50%")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Item { height: Theme.spacingS }
        }
    }

    // ---- limit editor ---------------------------------------------------------
    Dialog {
        id: limitDialog

        property int category: -1

        function openFor(cat, currentLimit) {
            category = cat
            limitSpin.value = Math.max(limitSpin.from, currentLimit / 100)
            open()
        }

        anchors.centerIn: parent
        modal: true
        title: qsTr("Monthly limit")

        standardButtons: Dialog.Cancel | Dialog.Save

        onAccepted: {
            root.bank.budgets.set_limit(category, Math.round(limitSpin.value * 100))
        }

        contentItem: RowLayout {
            spacing: Theme.spacingM
            Label { text: qsTr("Limit"); color: Theme.textMuted;
                    font.pixelSize: Theme.fontSizeSmall }
            SpinBox {
                id: limitSpin
                from: 100
                to: 50000000 / 100
                editable: true
            }
            Label { text: qsTr("RON / month"); color: Theme.textMuted;
                    font.pixelSize: Theme.fontSizeCaption }
        }
    }
}
