import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Global cross-bank ledger (plan §Phase 5): composable filters, result count,
// CSV export and a detail sheet with manual recategorization.
Page {
    id: root

    required property var bank
    property string initialSearch: ""

    title: qsTr("Transactions")

    background: null

    // ---- filter state ------------------------------------------------------
    property var _bankSel: []
    property var _catSel: []
    property string _preset: "all"

    function _syncFilters() {
        bank.ledger.bank_ids = root._bankSel
        bank.ledger.categories = root._catSel
        const today = new Date()
        switch (root._preset) {
        case "month":
            bank.ledger.from_date = new Date(today.getFullYear(), today.getMonth(), 1)
            bank.ledger.to_date = new Date(today.getFullYear(), today.getMonth() + 1, 0)
            break
        case "30d":
            const from = new Date(today); from.setDate(from.getDate() - 30)
            bank.ledger.from_date = from
            bank.ledger.to_date = today
            break
        default: // all time -> unset date range
            bank.ledger.clear_dates()
        }
    }

    Connections {
        target: root.bank ? root.bank.ledger : null
        function onFilter_changed() {
            countLabel.text = qsTr("%1 transaction(s)").arg(root.bank.ledger.rowCount)
        }
    }

    Component.onCompleted: {
        if (initialSearch.length > 0)
            searchField.text = initialSearch
        _syncFilters()
    }

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
            text: qsTr("Export CSV")
            flat: true
            icon.source: Theme.icon("receipt")
            onClicked: {
                const path = root.bank.export_ledger_csv()
                if (path.length > 0) {
                    root.bank.notifications.post("success", qsTr("CSV exported"), path)
                    copyPathLabel.text = path
                } else {
                    root.bank.notifications.post("warning", qsTr("Export failed"))
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        spacing: Theme.spacingM

        // ---- search + direction -------------------------------------------
        // The search field keeps a usable width by taking a row to itself once
        // the two filter combos no longer fit beside it.
        GridLayout {
            Layout.fillWidth: true
            columns: FormFactor.compact ? 2 : 3
            columnSpacing: Theme.spacingS
            rowSpacing: Theme.spacingS

            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.columnSpan: FormFactor.compact ? 2 : 1
                placeholderText: qsTr("Search counterparty, note or IBAN…")
                font.pixelSize: Theme.fontSizeSmall
                onTextChanged: root.bank.ledger.search_text = text
            }

            ComboBox {
                id: directionBox
                model: [qsTr("All"), qsTr("Money in"), qsTr("Money out")]
                font.pixelSize: Theme.fontSizeSmall
                onActivated: function (index) {
                    root.bank.ledger.direction = index - 1 // -1 all / 0 credit / 1 debit
                }
            }

            ComboBox {
                id: presetBox
                model: [qsTr("All time"), qsTr("This month"), qsTr("Last 30 days")]
                font.pixelSize: Theme.fontSizeSmall
                onActivated: function (index) {
                    root._preset = ["all", "month", "30d"][index]
                    root._syncFilters()
                }
            }
        }

        // ---- banks + categories chips ---------------------------------------
        // A Flow cannot report its wrapped height back to a layout: the layout
        // asks before assigning the width the wrapping depends on, so the chips
        // were laid out for one row and every later row overlapped the next.
        // The wrapper takes its width from the layout and its height from what
        // the Flow actually produced.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: chipFlow.implicitHeight

            Flow {
                id: chipFlow
                width: parent.width
                spacing: Theme.spacingS

                Repeater {
                    model: 4
                    delegate: BankChipToggle {
                        bank_id: index
                        selected: root._bankSel.includes(index)
                        onFilterToggled: function (on) {
                            const s = root._bankSel.slice()
                            if (on) s.push(index); else s.splice(s.indexOf(index), 1)
                            root._bankSel = s
                            root._syncFilters()
                        }
                    }
                }

                Rectangle { width: 1; height: 22; color: Theme.border }

                Repeater {
                    model: 14
                    delegate: CategoryChipToggle {
                        category: index
                        selected: root._catSel.includes(index)
                        onFilterToggled: function (on) {
                            const s = root._catSel.slice()
                            if (on) s.push(index); else s.splice(s.indexOf(index), 1)
                            root._catSel = s
                            root._syncFilters()
                        }
                    }
                }

                Button {
                    flat: true
                    text: qsTr("Clear filters")
                    font.pixelSize: Theme.fontSizeCaption
                    visible: root._bankSel.length || root._catSel.length ||
                             searchField.text.length || directionBox.currentIndex !== 0
                    onClicked: {
                        root._bankSel = []; root._catSel = []
                        searchField.clear(); directionBox.currentIndex = 0
                        presetBox.currentIndex = 0; root._preset = "all"
                        root.bank.ledger.clear()
                        root._syncFilters()
                    }
                }
            }
        }

        Label {
            id: countLabel
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeCaption
        }

        Label {
            id: copyPathLabel
            visible: text.length > 0
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeCaption
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        // ---- ledger list -----------------------------------------------------
        ListView {
            id: ledgerList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacingXS
            model: root.bank.ledger

            delegate: Frame {
                width: ledgerList.width
                leftPadding: Theme.spacingM

                background: Rectangle {
                    radius: Theme.radiusM
                    color: Theme.surface
                    // Highlight the row whose detail sheet is currently open.
                    border.color: (detailSheet.opened && detailSheet.rowData
                                   && detailSheet.rowData.reference === model.reference)
                                  ? Theme.accent : Theme.border
                }

                contentItem: RowLayout {
                    spacing: Theme.spacingS

                    BankBadge { bank_id: model.bank_id; size: 24 }

                    CategoryChip { category: model.category }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        Label {
                            text: model.counterparty
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: model.status === 0
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: Qt.formatDateTime(model.posted_at, "dd MMM yyyy · hh:mm")
                                  + (model.status === 0 ? qsTr(" · pending") : "")
                                  + (model.category_source === 2 ? qsTr(" · you edited") :
                                     model.category_source === 1 ? qsTr(" · auto") : "")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }

                    CurrencyLabel {
                        minor: model.signed_amount_minor
                        currency_code: model.currency_code
                        signedDisplay: true
                        pixelSize: Theme.fontSizeBody
                    }
                }

                TapHandler {
                    onTapped: detailSheet.openForRow(model)
                }
            }

            ScrollBar.vertical: ScrollBar {}

            EmptyState {
                anchors.centerIn: parent
                visible: ledgerList.count === 0
                iconSource: Theme.icon("search")
                title: qsTr("No transactions match")
                message: qsTr("Loosen the filters to see more of your history.")
            }
        }
    }

    // ---- chip toggles --------------------------------------------------------
    component BankChipToggle : AbstractButton {
        id: bchip
        property int bank_id: 0
        property bool selected: false
        signal filterToggled(bool on)

        implicitHeight: 26
        implicitWidth: row.implicitWidth + 20
        // Symmetric padding centres the content; the Control owns the geometry,
        // so the row must not anchor itself.
        padding: 0
        leftPadding: 10
        rightPadding: 10

        background: Rectangle {
            radius: Theme.radiusS
            color: bchip.selected ? Qt.alpha(Theme.bankColor(bchip.bank_id), 0.25) : Theme.surfaceAlt
            border.color: bchip.selected ? Theme.bankColor(bchip.bank_id) : Theme.border
        }
        contentItem: Row {
            id: row
            spacing: 6
            BankBadge {
                anchors.verticalCenter: parent.verticalCenter
                bank_id: bchip.bank_id
                size: 16
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: Theme.banks[bchip.bank_id].name.split(" ")[0]
                color: Theme.text
                font.pixelSize: Theme.fontSizeCaption
            }
        }
        onClicked: bchip.filterToggled(!bchip.selected)
    }

    component CategoryChipToggle : AbstractButton {
        id: cchip
        property int category: 13
        property bool selected: false
        signal filterToggled(bool on)

        function label(c) {
            return c === 0  ? qsTr("Salary")
                 : c === 1  ? qsTr("Groceries")
                 : c === 2  ? qsTr("Dining")
                 : c === 3  ? qsTr("Transport")
                 : c === 4  ? qsTr("Utilities")
                 : c === 5  ? qsTr("Shopping")
                 : c === 6  ? qsTr("Health")
                 : c === 7  ? qsTr("Entertainment")
                 : c === 8  ? qsTr("Travel")
                 : c === 9  ? qsTr("Fees")
                 : c === 10 ? qsTr("Transfer")
                 : c === 11 ? qsTr("Interest")
                 : c === 12 ? qsTr("Fx")
                 : qsTr("Other")
        }

        implicitHeight: 24
        implicitWidth: lbl.implicitWidth + 18
        padding: 0
        leftPadding: 9
        rightPadding: 9

        background: Rectangle {
            radius: Theme.radiusS
            color: cchip.selected ? Theme.categoryColors[cchip.category]
                                  : Qt.rgba(Theme.categoryColors[cchip.category].r,
                                            Theme.categoryColors[cchip.category].g,
                                            Theme.categoryColors[cchip.category].b,
                                            Theme.isDark ? 0.16 : 0.12)
            border.color: Theme.categoryColors[cchip.category]
        }
        contentItem: Label {
            id: lbl
            text: cchip.label(cchip.category)
            color: cchip.selected ? "#fff" : Theme.text
            font.pixelSize: Theme.fontSizeCaption
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: cchip.filterToggled(!cchip.selected)
    }

    // ---- detail sheet ----------------------------------------------------------
    Drawer {
        id: detailSheet

        property var rowData: null

        function openForRow(m) {
            rowData = {
                reference: m.reference, counterparty: m.counterparty,
                amount_minor: m.signed_amount_minor, currency_code: m.currency_code,
                posted_at: m.posted_at, status: m.status, iban: m.counterparty_iban,
                fx_note: m.fx_note, note: m.note, category: m.category,
                category_source: m.category_source, bank_id: m.bank_id
            }
            editCategory.currentIndex = rowData.category
            editNote.text = rowData.note
            open()
        }

        edge: Qt.BottomEdge
        height: parent.height * 0.62
        width: parent.width

        background: Rectangle {
            color: Theme.surface
            topRightRadius: Theme.radiusL
            topLeftRadius: Theme.radiusL
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacingM

            RowLayout {
                spacing: Theme.spacingS
                Layout.fillWidth: true

                BankBadge { bank_id: detailSheet.rowData ? detailSheet.rowData.bank_id : 0 }

                Label {
                    text: detailSheet.rowData ? detailSheet.rowData.counterparty : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeH3
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                ToolButton {
                    icon.source: Theme.icon("x")
                    onClicked: detailSheet.close()
                }
            }

            CurrencyLabel {
                minor: detailSheet.rowData ? detailSheet.rowData.amount_minor : 0
                currency_code: detailSheet.rowData ? detailSheet.rowData.currency_code : "RON"
                signedDisplay: true
                pixelSize: Theme.fontSizeH2
            }

            Label {
                visible: detailSheet.rowData && detailSheet.rowData.fx_note.length > 0
                text: detailSheet.rowData ? qsTr("Booked at: %1").arg(detailSheet.rowData.fx_note) : ""
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                visible: detailSheet.rowData && detailSheet.rowData.iban.length > 0
                text: detailSheet.rowData ? detailSheet.rowData.iban : ""
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeCaption
            }

            GridLayout {
                columns: 2
                columnSpacing: Theme.spacingM
                Layout.fillWidth: true

                Label { text: qsTr("Category"); color: Theme.textMuted;
                        font.pixelSize: Theme.fontSizeSmall }
                ComboBox {
                    id: editCategory
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontSizeSmall
                    model: [qsTr("Salary"), qsTr("Groceries"), qsTr("Dining"),
                            qsTr("Transport"), qsTr("Utilities"), qsTr("Shopping"),
                            qsTr("Health"), qsTr("Entertainment"), qsTr("Travel"),
                            qsTr("Fees"), qsTr("Transfer"), qsTr("Interest"),
                            qsTr("Fx"), qsTr("Other")]
                }

                Label { text: qsTr("Note"); color: Theme.textMuted;
                        font.pixelSize: Theme.fontSizeSmall }
                TextArea {
                    id: editNote
                    Layout.fillWidth: true
                    placeholderText: qsTr("Add a personal note…")
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                Layout.fillWidth: true
                highlighted: true
                enabled: detailSheet.rowData !== null
                text: qsTr("Save changes")
                onClicked: {
                    root.bank.amend_transaction(detailSheet.rowData.reference,
                                               editCategory.currentIndex,
                                               editNote.text)
                    root.bank.notifications.post("success", qsTr("Transaction updated"))
                    detailSheet.close()
                }
            }
        }
    }
}
