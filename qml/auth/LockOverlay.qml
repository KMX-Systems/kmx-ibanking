import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Full-window overlay shown when the session is locked (manual Ctrl+L or
// idle timeout). Unlock needs the password only — never a second OTP.
Rectangle {
    id: root

    required property var auth

    color: Theme.isDark ? "#e60d1117" : "#e6f0f4fa"

    function _unlock() {
        errorLabel.text = ""
        if (!root.auth.unlock(unlockField.text))
            errorLabel.text = qsTr("Enter your password to unlock.")
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(340, parent.width - 48)
        spacing: Theme.spacingM

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: Theme.icon("lock")
            sourceSize: Qt.size(40, 40)
            opacity: 0.8
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Session locked")
            color: Theme.text
            font.pixelSize: Theme.fontSizeH2
            font.bold: true
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Welcome back, %1").arg(root.auth.display_name)
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        TextField {
            id: unlockField
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            font.pixelSize: Theme.fontSizeBody
            focus: true
            onAccepted: root._unlock()
        }

        Label {
            id: errorLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: Theme.danger
            font.pixelSize: Theme.fontSizeSmall
        }

        Button {
            Layout.fillWidth: true
            highlighted: true
            text: qsTr("Unlock")
            onClicked: root._unlock()
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            flat: true
            text: qsTr("Log out instead")
            font.pixelSize: Theme.fontSizeCaption
            onClicked: root.auth.logout()
        }
    }
}
