import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank
import "../theme/Format.js" as Format

// Smart exchange hub (plan §Phase 7): live comparison across bank desks,
// why-lines, per-pair rate history and one-tap execution.
Page {
    id: root

    required property var bank

    background: null

    property int fromCurrency: 0 // RON
    property int toCurrency: 1   // EUR
    property real amountMajor: 1000
    readonly property int amount_minor: Math.round(amountMajor * 100)
    // Bumped on FX ticks / account changes so the comparison re-runs.
    property int revision: 0
    readonly property var options: {
        const _ = revision // dependency: recompute when balances/rates move
        return bank.advisor.advise(fromCurrency, toCurrency, amount_minor)
    }

    property var selectedRoute: null

    Connections {
        target: root.bank
        function onNet_worth_changed() { root.revision++ } // 1 Hz heartbeat
    }
    Connections {
        target: root.bank ? root.bank.account_model : null
        function onModel_rebuilt() { root.revision++ }
    }

    function _code(i) { return Theme.currency_code(i) }

    Component.onCompleted: selectRecommended()

    function selectRecommended() {
        selectedRoute = options.length > 0 ? options[0] : null
    }
    onOptionsChanged: if (!selectedRoute) selectRecommended()

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
            text: qsTr("Demo: FX shock")
            font.pixelSize: Theme.fontSizeCaption
            onClicked: {
                root.bank.fx_shock()
                root.bank.notifications.post("warning", qsTr("FX market shocked"),
                                             qsTr("Watch the ranking change"))
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        spacing: Theme.spacingM

        // ---- inputs ---------------------------------------------------------
        // Two currency pickers and an amount field do not share one handset row;
        // the amount drops to a line of its own.
        GridLayout {
            Layout.fillWidth: true
            columns: FormFactor.compact ? 3 : 4
            columnSpacing: Theme.spacingS
            rowSpacing: Theme.spacingS

            ComboBox {
                id: fromBox
                model: Theme.currencies
                currentIndex: root.fromCurrency
                onActivated: function (i) {
                    root.fromCurrency = i
                    if (root.toCurrency === i)
                        root.toCurrency = (i + 1) % 3
                    root.selectedRoute = null
                }
            }

            Label { text: qsTr("→"); color: Theme.textMuted }

            ComboBox {
                id: toBox
                model: Theme.currencies
                currentIndex: root.toCurrency
                onActivated: function (i) {
                    root.toCurrency = i
                    if (root.fromCurrency === i)
                        root.fromCurrency = (i + 2) % 3
                    root.selectedRoute = null
                }
            }

            TextField {
                Layout.fillWidth: true
                Layout.columnSpan: FormFactor.compact ? 3 : 1
                text: root.amountMajor > 0 ? root.amountMajor.toString() : ""
                placeholderText: qsTr("Amount in %1").arg(_code(root.fromCurrency))
                font: Theme.amountFont(Theme.fontSizeH3)
                validator: RegularExpressionValidator { regularExpression: /[0-9]{1,9}/ }
                onTextChanged: root.amountMajor = parseInt(text || "0")
            }
        }

        EmptyState {
            visible: root.options.length === 0
            iconSource: Theme.icon("trend")
            title: qsTr("No route available")
            message: qsTr("Connect more banks or pick a different pair — "
                          + "venues need accounts in both currencies.")
        }

        // ---- route comparison -------------------------------------------------
        Repeater {
            model: root.options

            delegate: Frame {
                id: routeCard
                required property int index
                required property var modelData

                readonly property bool isPick:
                    root.selectedRoute === root.options[routeCard.index]
                readonly property bool best: modelData.recommended === true
                readonly property var firstLeg:
                    modelData.legs.length ? modelData.legs[0] : null

                Layout.fillWidth: true

                background: Rectangle {
                    radius: Theme.radiusM
                    color: routeCard.best ? Qt.alpha(Theme.success,
                                                     Theme.isDark ? 0.14 : 0.10)
                                          : Theme.surface
                    border.color: routeCard.isPick ? Theme.accent
                                 : routeCard.best ? Theme.success : Theme.border
                    border.width: routeCard.isPick || routeCard.best ? 2 : 1
                }

                contentItem: RowLayout {
                    spacing: Theme.spacingM

                    BankBadge { bank_id: routeCard.firstLeg.bank_id; size: 30 }

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: Theme.spacingXS
                            Label {
                                text: Theme.banks[routeCard.firstLeg.bank_id].name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeBody
                                font.bold: true
                            }
                            Rectangle {
                                visible: routeCard.best
                                radius: Theme.radiusS; height: 18; width: bestLabel.implicitWidth + 12
                                color: Theme.success
                                Label {
                                    id: bestLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Best deal")
                                    color: "#fff"
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }
                            Label {
                                visible: routeCard.modelData.rate_limited_venue === true
                                text: qsTr("· cooling down")
                                color: Theme.warning
                                font.pixelSize: Theme.fontSizeCaption
                            }
                        }

                        Label {
                            property var args: routeCard.modelData.explanation_args
                            text: {
                                const key = routeCard.modelData.explanation_key
                                const legs = routeCard.modelData.legs
                                const via = args.length && key === "ROUTE_TWO_LEG_VIA"
                                            ? _code(args[0]) : ""
                                if (key === "ROUTE_DIRECT_NO_FEE")
                                    return qsTr("Direct exchange with no conversion fee.")
                                if (key === "ROUTE_TWO_LEG_VIA")
                                    return qsTr("Two-step exchange via %1 at this desk.").arg(via)
                                if (key === "ROUTE_SAVES_VS_DEFAULT" && args.length)
                                    return qsTr("Saves %1 vs the worst of your venues.")
                                        .arg(Format.money(args[0], _code(root.toCurrency)))
                                if (key === "ROUTE_LOWEST_SPREAD") {
                                    const r = legs[0].applied_rate
                                    return qsTr("Effective rate %1").arg(Number(r).toFixed(4))
                                }
                                return ""
                            }
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            visible: routeCard.modelData.legs.length === 2
                            text: {
                                const legs = routeCard.modelData.legs
                                if (legs.length !== 2) return ""
                                return Format.money(legs[0].in_minor, _code(legs[0].from))
                                     + qsTr("  step 1  →  ")
                                     + Format.money(legs[0].out_minor, _code(legs[0].to))
                                     + qsTr("   then   ")
                                     + Format.money(legs[1].in_minor, _code(legs[1].from))
                                     + qsTr("  step 2  →  ")
                                     + Format.money(legs[1].out_minor, _code(legs[1].to))
                            }
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        CurrencyLabel {
                            minor: routeCard.modelData.resulting_minor
                            currency_code: _code(root.toCurrency)
                            pixelSize: Theme.fontSizeH3
                        }
                        Label {
                            text: qsTr("you receive")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }

                    Button {
                        highlighted: routeCard.isPick
                        enabled: root.selectedRoute !== root.options[routeCard.index]
                        text: routeCard.isPick ? qsTr("Selected") : qsTr("Select")
                        onClicked: root.selectedRoute = root.options[routeCard.index]
                    }
                }

                TapHandler {
                    onTapped: root.selectedRoute = root.options[routeCard.index]
                }
            }
        }

        Button {
            visible: root.selectedRoute !== null
            Layout.alignment: Qt.AlignHCenter
            highlighted: true
            text: qsTr("Exchange %1 %2 now")
                  .arg(root.amountMajor).arg(_code(root.fromCurrency))
            onClicked: {
                if (root.bank.payments.execute_exchange(root.selectedRoute)) {
                    root.bank.notifications.post(
                        "success", qsTr("Exchanged"),
                        Format.money(root.selectedRoute.resulting_minor,
                                     _code(root.toCurrency)))
                    root.selectedRoute = null // no double-execution on stale rows
                    root.revision++
                }
            }
        }

        // ---- rate history + spread context ------------------------------------
        Frame {
            Layout.fillWidth: true
            background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                    border.color: Theme.border }
            contentItem: ColumnLayout {
                spacing: Theme.spacingS

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("%1/%2 mid-rate history")
                              .arg(_code(root.fromCurrency)).arg(_code(root.toCurrency))
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Number(root.bank.advisor_mid_rate(root.fromCurrency,
                                                              root.toCurrency)).toFixed(4)
                        color: Theme.accent
                        font: Theme.amountFont(Theme.fontSizeSmall)
                    }
                }

                Sparkline {
                    Layout.fillWidth: true
                    height: 56
                    values: root.bank.rate_history(root.fromCurrency, root.toCurrency)
                    lineColor: Theme.accent
                }

                Label {
                    text: qsTr("Desk spreads: KMX 0.15% · TBI 0.08% · BT 0.55%+fee · Erste 0.20/0.50%")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
