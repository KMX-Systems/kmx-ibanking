import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import KmxBank

// Login gate: username/password (+ optional PIN pad), demo hint card,
// lockout countdown. Drives the OTP dialog; success hands over to the shell.
Item {
    id: root

    required property var auth

    property string _passwordBuffer: ""
    property int _lockoutLeft: 0
    readonly property bool lockoutActive: _lockoutLeft > 0

    function _submit() {
        errorLabel.text = ""
        if (padToggle.checked)
            root.auth.authenticate(usernameField.text, _passwordBuffer)
        else
            root.auth.authenticate(usernameField.text, passwordField.text)
    }

    // Poll lockout state once per second while visible.
    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: root._lockoutLeft = root.auth.lockout_seconds_left()
    }

    Settings {
        id: loginSettings
        category: "auth"
        property string lastUser: ""
    }

    Connections {
        target: root.auth
        function onOtp_issued(code, validitySeconds) {
            if (!otpDialog.opened)
                otpDialog.openWith(code, validitySeconds) // dialog self-refreshes on resend
        }
        function onState_changed(state) {
            // Success or explicit logout both dismiss the OTP step.
            if (state === AuthService.Active || state === AuthService.LoggedOut)
                otpDialog.close()
        }
        function onLogin_failed(message) {
            errorLabel.text = message
        }
    }

    OtpDialog {
        id: otpDialog
        auth: root.auth
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bg }
            GradientStop { position: 1.0; color: Theme.isDark ? "#111827" : "#e8edf3" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 48)
        spacing: Theme.spacingM

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingS

            BankBadge { bank_id: 0; size: 40 }
            Label {
                text: "KMX"
                color: Theme.accent
                font.pixelSize: Theme.fontSizeH1
                font.bold: true
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Your banks, one place")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        Frame {
            Layout.fillWidth: true

            background: Rectangle {
                radius: Theme.radiusL
                color: Theme.surface
                border.color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacingM

                TextField {
                    id: usernameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Username")
                    font.pixelSize: Theme.fontSizeBody
                    enabled: !root.auth.busy && !root.lockoutActive
                    text: loginSettings.lastUser
                    onTextChanged: loginSettings.lastUser = text
                    onAccepted: root._submit()
                }

                TextField {
                    id: passwordField
                    visible: !padToggle.checked
                    Layout.fillWidth: true
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                    font.pixelSize: Theme.fontSizeBody
                    enabled: !root.auth.busy && !root.lockoutActive
                    onAccepted: root._submit()
                }

                TextField {
                    id: pinEchoField
                    visible: padToggle.checked
                    Layout.fillWidth: true
                    readOnly: true
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Enter your code with the pad")
                    font.pixelSize: Theme.fontSizeH3
                    onVisibleChanged:
                        if (visible) pinEchoField.text = "•".repeat(root._passwordBuffer.length)
                }

                Switch {
                    id: padToggle
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Use numeric pad")
                    font.pixelSize: Theme.fontSizeCaption
                }

                PinPad {
                    id: pinPad
                    visible: padToggle.checked
                    Layout.alignment: Qt.AlignHCenter
                    onDigitPressed: (d) => {
                        if (root._passwordBuffer.length < 12) {
                            root._passwordBuffer += d
                            pinEchoField.text = "•".repeat(root._passwordBuffer.length)
                        }
                    }
                    onBackspacePressed: {
                        root._passwordBuffer = root._passwordBuffer.slice(0, -1)
                        pinEchoField.text = "•".repeat(root._passwordBuffer.length)
                    }
                    onSubmitRequested: root._submit()
                }

                Button {
                    Layout.fillWidth: true
                    text: root.auth.busy ? qsTr("Checking…") : qsTr("Log in")
                    highlighted: true
                    enabled: !root.auth.busy && !root.lockoutActive
                    onClicked: root._submit()
                }

                Label {
                    id: errorLabel
                    Layout.fillWidth: true
                    visible: text.length > 0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.lockoutActive
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeSmall
                    text: qsTr("Too many attempts — locked for %1 s.")
                          .arg(root._lockoutLeft)
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    text: qsTr("Demo credentials: any non-empty username and password.")
                }
            }
        }
    }
}
