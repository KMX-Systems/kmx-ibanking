import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

// Bottom-right notification stack. Call `notify(level, text, actionLabel, cb)`
// from anywhere under the shell. Levels: info | success | warning | critical.
Item {
    id: root

    property int maxVisible: 4
    property var _model: []

    // Distance to keep clear at the bottom; the mobile shell passes its nav bar
    // height so toasts never cover the destinations they point at.
    property int bottomInset: 0

    function notify(level, text, actionLabel, callback) {
        _model.push({ level: level || "info", text: text,
                      actionLabel: actionLabel || "", cb: callback || null })
        while (_model.length > maxVisible)
            _model.shift()
        _modelChanged() // trigger re-evaluation of the repeater binding
        dismissTimer.restart()
    }

    // Repeater owns its delegates, so a toast cannot remove itself; dropping the
    // entry from the model is what takes it off screen.
    function _dismiss(entry) {
        const at = _model.indexOf(entry)
        if (at < 0)
            return
        _model.splice(at, 1)
        _modelChanged()
        if (_model.length === 0)
            dismissTimer.stop()
    }

    function _dismissOldest() {
        if (_model.length === 0) {
            dismissTimer.stop()
            return
        }
        _model.shift()
        _modelChanged()
    }

    anchors.fill: parent
    z: 900

    Timer {
        id: dismissTimer
        interval: 3800
        repeat: true
        onTriggered: root._dismissOldest()
    }

    Column {
        id: stack
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        anchors.bottomMargin: Theme.spacingL + root.bottomInset
        spacing: Theme.spacingS
        width: Math.min(360, parent.width - 2 * Theme.spacingL)

        Repeater {
            model: root._model.slice().reverse()

            delegate: Rectangle {
                id: toastCard
                required property var modelData
                readonly property color tone:
                    modelData.level === "success" ? Theme.success :
                    modelData.level === "warning" ? Theme.warning :
                    modelData.level === "critical" ? Theme.danger : Theme.info

                width: stack.width
                height: row.implicitHeight + 2 * Theme.spacingM
                radius: Theme.radiusM
                color: Theme.surface
                border.color: toastCard.tone
                border.width: 1
                opacity: 0

                Component.onCompleted: opacity = 1
                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.durationFast
                        onFinished: if (toastCard.opacity === 0)
                                        root._dismiss(toastCard.modelData)
                    }
                }

                RowLayout {
                    id: row
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM
                    spacing: Theme.spacingS

                    Rectangle {
                        Layout.preferredWidth: 4
                        Layout.fillHeight: true
                        radius: 2
                        color: toastCard.tone
                    }

                    Label {
                        Layout.fillWidth: true
                        text: toastCard.modelData.text
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        visible: toastCard.modelData.actionLabel.length > 0
                        text: toastCard.modelData.actionLabel
                        flat: true
                        onClicked: {
                            if (toastCard.modelData.cb)
                                toastCard.modelData.cb()
                            root._dismiss(toastCard.modelData)
                        }
                    }
                }

                TapHandler { onTapped: toastCard.opacity = 0 }
            }
        }
    }
}
