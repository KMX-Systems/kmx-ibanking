import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import KmxBank

// Settings (plan §Phase 10): appearance, language, security shortcuts.
Page {
    id: root

    required property var bank

    background: null

    Settings {
        id: uiSettings
        category: "ui"
        property string mode: "dark"
        property string language: "en"
    }

    Component.onCompleted: {
        languageBox.currentIndex = uiSettings.language === "ro" ? 1 : 0
    }

    function applyLanguage(index) {
        const locale = index === 1 ? "ro" : "en"
        if (locale !== uiSettings.language) {
            uiSettings.language = locale
            root.bank.set_language(locale)
            root.bank.notifications.post("success", qsTr("Language changed"),
                                         qsTr("Interface language updated."))
        }
    }

    title: qsTr("Settings")

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

            // ---- appearance -------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingM

                    Label {
                        text: qsTr("Appearance")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                    }

                    RowLayout {
                        spacing: Theme.spacingM
                        Label {
                            text: qsTr("Theme")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Button {
                            text: Theme.isDark ? qsTr("Switch to light") : qsTr("Switch to dark")
                            onClicked: Theme.toggleMode()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        spacing: Theme.spacingM
                        Label {
                            text: qsTr("Display currency")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        ComboBox {
                            model: Theme.currencies
                            currentIndex: root.bank.display_currency
                            onActivated: function (i) {
                                root.bank.display_currency = i
                                currentIndex = root.bank.display_currency
                            }
                        }
                    }

                    RowLayout {
                        spacing: Theme.spacingM
                        Label {
                            text: qsTr("Interface language")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        ComboBox {
                            id: languageBox
                            model: ["English", "Română"]
                            onActivated: function (i) { root.applyLanguage(i) }
                        }
                    }
                }
            }

            // ---- shell / form factor -----------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    Label {
                        text: qsTr("Interface")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Currently running the %1 shell.")
                              .arg(UiConfig.mobile ? qsTr("mobile") : qsTr("desktop"))
                              + (UiConfig.emulated
                                 ? qsTr(" Emulating %1 — %2×%3 at %4 PPI, a %5×%6 canvas.")
                                   .arg(UiConfig.device_label)
                                   .arg(UiConfig.physical_width).arg(UiConfig.physical_height)
                                   .arg(UiConfig.ppi)
                                   .arg(UiConfig.logical_width).arg(UiConfig.logical_height)
                                 : "")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        spacing: Theme.spacingM

                        Label {
                            text: qsTr("Shell at next start")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        ComboBox {
                            id: shellBox
                            model: [qsTr("Desktop"), qsTr("Mobile"), qsTr("Automatic")]
                            currentIndex: UiConfig.mobile ? 1 : 0
                            onActivated: function (i) {
                                UiConfig.set_preferred_mode(["desktop", "mobile", "auto"][i])
                                root.bank.notifications.post(
                                            "info", qsTr("Shell preference saved"),
                                            qsTr("Restart the app to switch."))
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        spacing: Theme.spacingM

                        Label {
                            text: qsTr("Emulated device")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        // Presets describe physical panels; the canvas the app
                        // lays out on is that grid divided by the pixel ratio.
                        ComboBox {
                            id: deviceBox
                            Layout.fillWidth: true
                            textRole: "label"
                            model: UiConfig.devices
                            currentIndex: UiConfig.device_index_of(UiConfig.device_key)
                            onActivated: function (i) {
                                UiConfig.set_preferred_device(UiConfig.devices[i].key)
                                root.bank.notifications.post(
                                            "info", qsTr("Device preference saved"),
                                            qsTr("Restart the app to apply."))
                            }
                        }
                    }
                }
            }

            // ---- security ----------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingS

                    Label {
                        text: qsTr("Security")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.bold: true
                    }

                    Label {
                        text: qsTr("Idle auto-lock is always on (5 minutes). "
                                   + "Lock manually any time with Ctrl+L.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Button {
                            icon.source: Theme.icon("lock")
                            text: qsTr("Lock now")
                            onClicked: root.bank.auth.lock()
                        }
                        Button {
                            flat: true
                            text: qsTr("Log out")
                            onClicked: root.bank.auth.logout()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Button {
                        flat: true
                        text: qsTr("Manage linked banks")
                        icon.source: Theme.icon("plug")
                        onClicked: shellRoute.open_route("connections")
                    }
                }
            }

            // ---- about ---------------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Theme.surface;
                                        border.color: Theme.border }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingXS
                    RowLayout {
                        spacing: Theme.spacingS
                        BankBadge { bank_id: 0; size: 26 }
                        Label {
                            text: qsTr("KMX meta-banking demo")
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                    }
                    Label {
                        text: qsTr("Simulated data only — no real banks are contacted. "
                                   + "All balances, transactions and institutions are fictional "
                                   + "recreations for demonstration purposes.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("Version %1").arg(Qt.application.version || "0.1")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }
            }

            // ---- data -----------------------------------------------------------
            Frame {
                Layout.fillWidth: true
                background: Rectangle { radius: Theme.radiusM; color: Qt.alpha(Theme.danger,
                                                                                   Theme.isDark ? 0.14 : 0.10);
                                        border.color: Theme.danger }
                contentItem: ColumnLayout {
                    spacing: Theme.spacingXS

                    Label {
                        text: qsTr("Demo data")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Label {
                        text: qsTr("Restart the app to reset every balance and transaction "
                                   + "back to the seeded dataset. UI preferences persist.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Item { height: Theme.spacingXL }
        }
    }
}
