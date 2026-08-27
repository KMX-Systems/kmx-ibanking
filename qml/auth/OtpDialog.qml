import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Second authentication step: 6-digit code with 60 s validity, resend
// countdown, and a simulated "SMS" preview card (demo aid per plan §4).
Dialog {
    id: root

    required property var auth

    property string _code: ""
    property int _secondsLeft: 0

    function openWith(code, validitySeconds) {
        _code = code
        _secondsLeft = validitySeconds
        codeField.clear()
        errorLabel.text = ""
        countdownTimer.restart()
        open()
    }

    function _verify() {
        if (codeField.text.length === 6)
            root.auth.verify_otp(codeField.text)
    }

    anchors.centerIn: parent
    modal: true
    title: qsTr("Two-factor verification")
    standardButtons: Dialog.NoButton
    closePolicy: Popup.NoAutoClose

    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 48 : 420)

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            root._secondsLeft = Math.max(0, root._secondsLeft - 1)
            if (root._secondsLeft === 0)
                countdownTimer.stop()
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingM

        Label {
            Layout.fillWidth: true
            text: qsTr("We sent a 6-digit code to your phone. It expires in %1 s.")
                  .arg(root._secondsLeft)
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        // Simulated SMS preview — the demo's stand-in for a real SMS/app push.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: smsRow.implicitHeight + 2 * Theme.spacingM
            radius: Theme.radiusM
            color: Theme.surfaceAlt
            border.color: Theme.border

            RowLayout {
                id: smsRow
                anchors.fill: parent
                anchors.margins: Theme.spacingM
                spacing: Theme.spacingM

                ColumnLayout {
                    spacing: 2
                    Label {
                        text: qsTr("KMX Bank · now")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                    }
                    Label {
                        text: qsTr("Your verification code is %1").arg(root._code)
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    flat: true
                    text: qsTr("Autofill")
                    font.pixelSize: Theme.fontSizeCaption
                    onClicked: {
                        codeField.text = root._code
                        root._verify()
                    }
                }
            }
        }

        TextField {
            id: codeField
            Layout.fillWidth: true
            placeholderText: qsTr("••••••")
            font: Theme.amountFont(Theme.fontSizeH2)
            maximumLength: 6
            horizontalAlignment: Text.AlignHCenter
            enabled: !root.auth.busy
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
            onAccepted: root._verify()
        }

        Label {
            id: errorLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: Theme.danger
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingS

            Button {
                Layout.fillWidth: true
                enabled: !root.auth.busy
                text: qsTr("Back to login")
                flat: true
                onClicked: {
                    root.auth.logout()
                    root.close()
                }
            }

            Button {
                Layout.fillWidth: true
                enabled: root._secondsLeft <= 45 && !root.auth.busy
                text: qsTr("Resend code")
                flat: true
                onClicked: {
                    errorLabel.text = ""
                    root.auth.resend_otp()
                }
            }

            Button {
                Layout.fillWidth: true
                highlighted: true
                enabled: codeField.text.length === 6 && !root.auth.busy
                text: root.auth.busy ? qsTr("Verifying…") : qsTr("Verify")
                onClicked: root._verify()
            }
        }
    }

    Connections {
        target: root.auth
        function onLogin_failed(message) { errorLabel.text = message }
        function onOtp_issued(code, validitySeconds) {
            // Covers resend while the dialog is already open.
            root._code = code
            root._secondsLeft = validitySeconds
            codeField.clear()
            errorLabel.text = ""
            countdownTimer.restart()
        }
    }

    onClosed: countdownTimer.stop()
}
