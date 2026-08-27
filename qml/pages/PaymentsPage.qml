import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank
import "../theme/Format.js" as Format

// Payments hub (plan §Phase 6): transfer wizard with validation pipeline,
// animated receipt, scheduled payments tab with the demo trigger.
Page {
    id: root

    required property var bank

    background: null

    // ---- wizard state -------------------------------------------------------
    property int step: 0
    property int source_account_id: -1
    property string source_name: ""
    property string sourceCurrency: "RON"

    property int beneficiaryRow: -1
    property bool newBeneficiary: false
    property string destIban: ""
    property string destName: ""
    property bool ibanValid: false

    property real amountMajor: 0
    readonly property int amount_minor: Math.round(amountMajor * 100)
    property bool busy: false
    property var receipt: null
    property string errorText: ""

    property var scheduledRows: bank.payments.scheduled_list()
    Connections {
        target: root.bank.payments
        function onScheduled_changed() { root.scheduledRows = root.bank.payments.scheduled_list() }
    }

    function resetWizard() {
        step = 0; source_account_id = -1; beneficiaryRow = -1
        newBeneficiary = false; destIban = ""; destName = ""
        amountMajor = 0; noteField.text = ""; repeatMonthly.checked = false
        receipt = null; errorText = ""; ibanValid = false
    }

    function currentRequest() {
        let name = destName
        let iban = destIban
        if (!newBeneficiary && beneficiaryRow >= 0) {
            const b = root.bank.beneficiaries.get(beneficiaryRow)
            name = b.name
            iban = b.iban
        }
        return { source_account_id: source_account_id, beneficiary_iban: iban,
                 beneficiary_name: name, amount_minor: amount_minor,
                 note: noteField.text }
    }

    Connections {
        target: root.bank.payments
        function onTransfer_completed(receiptMap) {
            root.busy = false
            root.receipt = receiptMap
            root.step = 3
            root.bank.notifications.post("success", qsTr("Transfer sent"),
                                         qsTr("%1 → %2").arg(root.source_name)
                                             .arg(receiptMap.beneficiary_name))
        }
        function onTransfer_failed(code, message) {
            root.busy = false
            root.step = 2
            root.errorText = message
            root.bank.notifications.post("critical", qsTr("Transfer failed"), message)
        }
    }


    title: qsTr("Payments")

    header: SectionHeader {
        title: root.title
        pageHeader: true
        actionText: step === 3 ? "" : qsTr("New transfer")
        onActionTriggered: root.resetWizard()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: FormFactor.pageMargin
        spacing: Theme.spacingM

        TabBar {
            id: modeTabs
            Layout.fillWidth: true
            TabButton { text: qsTr("New transfer") }
            TabButton { text: qsTr("Scheduled") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: modeTabs.currentIndex

            // ==================== NEW TRANSFER WIZARD ====================
            ScrollView {
                id: wizardScroller
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: wizardScroller.availableWidth
                    spacing: Theme.spacingM

                    StepperBar {
                        id: stepper
                        steps: [qsTr("Source"), qsTr("Destination"), qsTr("Amount"), qsTr("Done")]
                        current: Math.min(root.step, 3)
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        currentIndex: root.step

                        // ---- step 0: source ---------------------------------
                        ColumnLayout {
                            spacing: Theme.spacingS

                            Repeater {
                                model: root.bank.account_model

                                delegate: Loader {
                                    required property int index
                                    required property var model
                                    Layout.fillWidth: true
                                    sourceComponent: model.row_type === 0 ? skip : srcCard

                                    Component { id: skip; Item { height: 0 } }

                                    Component {
                                        id: srcCard

                                        AccountPickCard {
                                            account_name: model.name
                                            masked_iban: model.masked_iban
                                            balance_minor: model.balance_minor
                                            currency_code: model.currency_code
                                            bank_id: model.bank_id
                                            selected: root.source_account_id === model.account_id
                                            onSelect: {
                                                root.source_account_id = model.account_id
                                                root.source_name = model.name
                                                root.sourceCurrency = model.currency_code
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: qsTr("Continue")
                                    highlighted: true
                                    enabled: root.source_account_id > 0
                                    onClicked: root.step = 1
                                }
                            }
                        }

                        // ---- step 1: destination ------------------------------
                        ColumnLayout {
                            spacing: Theme.spacingS

                            Switch {
                                id: newBenSwitch
                                text: qsTr("New beneficiary")
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            ColumnLayout {
                                visible: !newBenSwitch.checked
                                spacing: Theme.spacingXS
                                Layout.fillWidth: true

                                Label {
                                    visible: root.bank.beneficiaries.rowCount() === 0
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSmall
                                    text: qsTr("No beneficiaries yet — add one below.")
                                }

                                ListView {
                                    id: benList
                                    implicitHeight: Math.min(240, contentHeight)
                                    interactive: contentHeight > implicitHeight
                                    clip: true
                                    spacing: Theme.spacingXS
                                    Layout.fillWidth: true
                                    model: root.bank.beneficiaries

                                    delegate: ItemDelegate {
                                        id: benRow
                                        required property int index
                                        required property string name
                                        required property string iban
                                        required property string currency_code
                                        required property bool favorite

                                        width: benList.width
                                        height: 46

                                        background: Rectangle {
                                            radius: Theme.radiusS
                                            color: benRow.index === root.beneficiaryRow
                                                   ? Qt.alpha(Theme.accent, 0.18)
                                                   : benRow.hovered ? Theme.surfaceAlt : "transparent"
                                            border.color: benRow.index === root.beneficiaryRow
                                                          ? Theme.accent : Theme.border
                                        }

                                        contentItem: RowLayout {
                                            spacing: Theme.spacingS

                                            Label {
                                                text: benRow.favorite ? "★" : "☆"
                                                color: Theme.warning
                                                font.pixelSize: Theme.fontSizeBody
                                                TapHandler {
                                                    onTapped: root.bank.beneficiaries
                                                                  .toggle_favorite(benRow.index)
                                                }
                                            }

                                            ColumnLayout {
                                                spacing: 0
                                                Layout.fillWidth: true
                                                Label { text: benRow.name; color: Theme.text;
                                                        font.pixelSize: Theme.fontSizeSmall }
                                                Label { text: benRow.iban; color: Theme.textMuted;
                                                        font.pixelSize: Theme.fontSizeCaption;
                                                        elide: Text.ElideMiddle;
                                                        Layout.fillWidth: true }
                                            }

                                            Label {
                                                text: benRow.currency_code
                                                color: Theme.textMuted
                                                font.pixelSize: Theme.fontSizeCaption
                                            }
                                        }

                                        onClicked: {
                                            root.newBeneficiary = false
                                            newBenSwitch.checked = false
                                            root.beneficiaryRow = benRow.index
                                            root.destIban = benRow.iban
                                            root.destName = benRow.name
                                            root.ibanValid = true
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                visible: newBenSwitch.checked
                                spacing: Theme.spacingXS
                                Layout.fillWidth: true

                                TextField {
                                    id: newName
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Beneficiary name")
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                TextField {
                                    id: newIban
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("RO… IBAN (paste without spaces)")
                                    font.pixelSize: Theme.fontSizeSmall
                                    onTextChanged: {
                                        root.destIban = text
                                        root.destName = newName.text
                                        root.ibanValid = root.bank.is_valid_iban(text) &&
                                                text.startsWith("RO")
                                        ibanFeedback.text = text.length < 5 ? "" :
                                                            root.ibanValid ? qsTr("✓ Checksum OK")
                                                                           : qsTr("✗ Invalid IBAN checksum")
                                        ibanFeedback.color = root.ibanValid ? Theme.success : Theme.danger
                                    }
                                }
                                Label { id: ibanFeedback; font.pixelSize: Theme.fontSizeCaption }
                            }

                            RowLayout {
                                Button {
                                    text: qsTr("Back")
                                    flat: true
                                    onClicked: root.step = 0
                                }
                                Button {
                                    text: newBenSwitch.checked && root.bank.beneficiaries.rowCount() >= 0
                                          ? qsTr("Save to beneficiaries") : qsTr("Continue")
                                    highlighted: !newBenSwitch.checked
                                    enabled: newBenSwitch.checked
                                             ? (newName.text.length > 1 && root.ibanValid)
                                             : root.beneficiaryRow >= 0
                                    onClicked: {
                                        if (newBenSwitch.checked) {
                                            root.bank.beneficiaries.add(newName.text, newIban.text, 0)
                                            root.beneficiaryRow = 0
                                            root.destIban = newIban.text
                                            root.destName = newName.text
                                            newName.clear(); newIban.clear()
                                            newBenSwitch.checked = false
                                            root.step = 2
                                        } else {
                                            root.step = 2
                                        }
                                    }
                                }
                            }
                        }

                        // ---- step 2: amount & review ---------------------------
                        ColumnLayout {
                            spacing: Theme.spacingM

                            Label {
                                text: qsTr("From %1 · to %2").arg(root.source_name)
                                                         .arg(root.destName.length
                                                              ? root.destName : root.destIban)
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            TextField {
                                id: amountField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Amount in %1").arg(root.sourceCurrency)
                                font: Theme.amountFont(Theme.fontSizeH2)
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{1,7}([.,][0-9]{0,2})?/ }
                                onTextChanged: {
                                    const normalized = text.replace(',', '.')
                                    root.amountMajor = parseFloat(normalized || "0")
                                }
                            }

                            TextArea {
                                id: noteField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Note (optional)")
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            // SmartRoutePanel (plan §7): live comparison when the
                            // destination account is in another currency.
                            Frame {
                                id: routePanel

                                readonly property int destCcy:
                                    root.bank.currency_for_iban(root.destIban)
                                readonly property bool crossCurrency:
                                    destCcy >= 0 && _code(destCcy) !== root.sourceCurrency
                                readonly property var routes:
                                    crossCurrency && root.amount_minor > 0
                                        ? root.bank.advisor.advise(
                                              Theme.currencies.indexOf(root.sourceCurrency),
                                              destCcy, root.amount_minor)
                                        : []

                                visible: crossCurrency
                                Layout.fillWidth: true

                                background: Rectangle {
                                    radius: Theme.radiusM
                                    color: Qt.alpha(Theme.accent, 0.10)
                                    border.color: Theme.accent
                                }

                                contentItem: ColumnLayout {
                                    spacing: Theme.spacingS

                                    Label {
                                        text: qsTr("Smart routing · %1 %2 → %3")
                                              .arg(root.amount_minor / 100.0)
                                              .arg(root.sourceCurrency)
                                              .arg(Theme.currency_code(routePanel.destCcy >= 0 ? routePanel.destCcy : 0))
                                        color: Theme.text
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: routePanel.routes
                                        delegate: RowLayout {
                                            required property var modelData
                                            spacing: Theme.spacingS
                                            Layout.fillWidth: true

                                            Rectangle { width: 8; height: 8; radius: 4;
                                                        color: modelData.recommended ? Theme.success : Theme.border }
                                            Label {
                                                text: Theme.banks[modelData.legs[0].bank_id].name.split(" ")[0]
                                                color: Theme.text
                                                font.pixelSize: Theme.fontSizeCaption
                                                Layout.preferredWidth: 60
                                            }
                                            Label {
                                                text: qsTr("you'd get")
                                                color: Theme.textMuted
                                                font.pixelSize: Theme.fontSizeCaption
                                            }
                                            CurrencyLabel {
                                                minor: modelData.resulting_minor
                                                currency_code: Theme.currency_code(root.bank.currency_for_iban(root.destIban))
                                                pixelSize: Theme.fontSizeCaption
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: modelData.explanation_key === "ROUTE_TWO_LEG_VIA"
                                                      ? qsTr("via %1").arg(modelData.explanation_args[0] ?? "")
                                                      : modelData.recommended ? qsTr("cheapest") : ""
                                                color: modelData.recommended ? Theme.success : Theme.textMuted
                                                font.pixelSize: Theme.fontSizeCaption
                                            }
                                        }
                                    }

                                    Label {
                                        visible: routePanel.routes.length === 0
                                        text: qsTr("No funded venue for this pair yet.")
                                        color: Theme.warning
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                }
                            }

                            CheckBox {
                                id: repeatMonthly
                                text: qsTr("Repeat monthly as a standing order")
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            SpinBox {
                                id: daySpin
                                visible: repeatMonthly.checked
                                from: 1; to: 28; value: 1
                                editable: true
                            }

                            Label {
                                id: errorLabel
                                visible: root.errorText.length > 0
                                text: root.errorText
                                color: Theme.danger
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Button {
                                    text: qsTr("Back")
                                    flat: true
                                    enabled: !root.busy
                                    onClicked: { root.errorText = ""; root.step = 1 }
                                }
                                Button {
                                    Layout.fillWidth: true
                                    highlighted: true
                                    enabled: !root.busy && root.amount_minor >= 100 &&
                                             root.source_account_id > 0 &&
                                             root.destIban.length > 0
                                    text: root.busy ? qsTr("Processing…") :
                                          repeatMonthly.checked ? qsTr("Schedule payment")
                                                                : qsTr("Send %1")
                                                                    .arg(Format.money(root.amount_minor,
                                                                                      root.sourceCurrency))
                                    onClicked: {
                                        root.errorText = ""
                                        const request = root.currentRequest()
                                        if (repeatMonthly.checked) {
                                            const res = root.bank.payments.schedule_from_map(request, daySpin.value)
                                            if (res.ok === true) {
                                                root.bank.notifications.post(
                                                    "success", qsTr("Standing order created"),
                                                    qsTr("Runs on day %1 monthly").arg(daySpin.value))
                                                root.resetWizard()
                                                modeTabs.currentIndex = 1
                                            } else {
                                                root.errorText = res.message.toString()
                                            }
                                        } else {
                                            root.busy = true
                                            root.step = 2
                                            skeletonTimer.restart()
                                            if (!root.bank.payments.submit(request)) {
                                                root.busy = false
                                                skeletonTimer.stop()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ---- step 3: processing / receipt ----------------------
                        StackLayout {
                            currentIndex: root.receipt ? 1 : 0

                            ColumnLayout {
                                spacing: Theme.spacingL
                                SkeletonBox { Layout.fillWidth: true; height: 22 }
                                SkeletonBox { Layout.preferredWidth: 220; height: 22 }
                                SkeletonBox { Layout.fillWidth: true; height: 64 }
                                SkeletonBox { Layout.preferredWidth: 160; height: 22 }
                                Label {
                                    text: qsTr("Booking your transfer…")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            ColumnLayout {
                                spacing: Theme.spacingM

                                Image {
                                    Layout.alignment: Qt.AlignHCenter
                                    source: Theme.icon("plus")
                                    sourceSize: Qt.size(42, 42)
                                    rotation: 45
                                    opacity: 0.85
                                }

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Transfer sent")
                                    color: Theme.success
                                    font.pixelSize: Theme.fontSizeH2
                                    font.bold: true
                                }

                                Frame {
                                    Layout.fillWidth: true
                                    background: Rectangle { radius: Theme.radiusM;
                                                            color: Theme.surfaceAlt }
                                    contentItem: ColumnLayout {
                                        spacing: Theme.spacingXS
                                        visible: root.receipt !== null

                                        Label {
                                            text: root.receipt ?
                                                  Format.money(root.receipt.amount_minor,
                                                               root.receipt.currency_code) : ""
                                            color: Theme.text
                                            font: Theme.amountFont(Theme.fontSizeH2)
                                        }
                                        Label {
                                            text: root.receipt ?
                                                  qsTr("%1 → %2").arg(root.receipt.source_name)
                                                      .arg(root.receipt.beneficiary_name) : ""
                                            color: Theme.text
                                            font.pixelSize: Theme.fontSizeSmall
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: root.receipt ?
                                                  qsTr("Reference %1 · booked %2")
                                                      .arg(root.receipt.reference)
                                                      .arg(Qt.formatDateTime(root.receipt.booked_at,
                                                                             "dd MMM hh:mm")) : ""
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeCaption
                                        }
                                    }
                                }

                                Button {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Done")
                                    onClicked: root.resetWizard()
                                }
                            }
                        }
                    }

                    Timer { id: skeletonTimer } // reserved for forced-min-duration polish
                }
            }

            // ==================== SCHEDULED TAB ============================
            ColumnLayout {
                spacing: Theme.spacingM

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Upcoming standing orders")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Button {
                        flat: true
                        text: qsTr("Demo: run due now")
                        onClicked: {
                            const n = root.bank.payments.run_due_now(true)
                            root.bank.notifications.post(
                                n > 0 ? "success" : "info",
                                n > 0 ? qsTr("%1 standing order(s) executed").arg(n)
                                      : qsTr("Nothing due — schedule one first"))
                        }
                    }
                }

                Repeater {
                    model: root.scheduledRows

                    delegate: Frame {
                        required property var modelData
                        Layout.fillWidth: true

                        background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                                border.color: Theme.border }
                        contentItem: RowLayout {
                            spacing: Theme.spacingM
                            Image {
                                source: Theme.icon("swap")
                                sourceSize: Qt.size(18, 18)
                                opacity: 0.8
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                Label {
                                    text: modelData.beneficiary_name
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.bold: true
                                }
                                Label {
                                    text: qsTr("Day %1 · next %2")
                                          .arg(modelData.day_of_month)
                                          .arg(Qt.formatDate(modelData.next_run, "dd MMM yyyy"))
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }
                            CurrencyLabel {
                                minor: -modelData.amount_minor
                                currency_code: root.bank.account_currency_code(
                                                  root.source_account_id >= 0 ? root.source_account_id : 0)
                                tone: "neutral"
                                pixelSize: Theme.fontSizeSmall
                            }
                            ToolButton {
                                icon.source: Theme.icon("x")
                                Accessible.name: qsTr("Cancel standing order")
                                onClicked: {
                                    root.bank.payments.cancel_scheduled(modelData.id)
                                    root.bank.notifications.post("info",
                                                                 qsTr("Standing order cancelled"))
                                }
                            }
                        }
                    }
                }

                EmptyState {
                    visible: root.scheduledRows.length === 0
                    title: qsTr("No standing orders")
                    message: qsTr("Create one from the New transfer tab by ticking "
                                  + "\"Repeat monthly\".")
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    component StepperBar : RowLayout {
        id: bar
        property var steps: []
        property int current: 0
        spacing: Theme.spacingS

        Repeater {
            model: bar.steps
            delegate: RowLayout {
                id: stepChip
                required property int index
                required property string modelData
                spacing: 6
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: stepChip.index <= bar.current ? Theme.accent : Theme.border
                }
                Label {
                    visible: !FormFactor.compact || stepChip.index === bar.current
                    text: stepChip.modelData
                    color: stepChip.index <= bar.current ? Theme.text : Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                }
                Rectangle { visible: stepChip.index < (bar.steps.length - 1)
                            width: FormFactor.compact ? 12 : 24
                            height: 1; color: Theme.border }
            }
        }
    }

    component AccountPickCard : Frame {
        id: pickCard
        signal select()

        property string account_name
        property string masked_iban
        property int balance_minor
        property string currency_code
        property int bank_id: 0
        property bool selected: false

        Layout.fillWidth: true

        background: Rectangle {
            radius: Theme.radiusM
            color: parent.selected ? Qt.alpha(Theme.accent, 0.15) : Theme.surface
            border.color: parent.selected ? Theme.accent : Theme.border
        }

        contentItem: RowLayout {
            spacing: Theme.spacingS
            BankBadge { bank_id: pickCard.bank_id; size: 26 }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: pickCard.account_name; color: Theme.text;
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true;
                        elide: Text.ElideRight }
                Label { Layout.fillWidth: true; text: pickCard.masked_iban; color: Theme.textMuted;
                        font.pixelSize: Theme.fontSizeCaption; elide: Text.ElideRight }
            }
            CurrencyLabel { minor: pickCard.balance_minor;
                            currency_code: pickCard.currency_code;
                            pixelSize: Theme.fontSizeSmall }
        }

        TapHandler { onTapped: pickCard.select() }
    }
}
