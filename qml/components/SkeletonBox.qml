import QtQuick
import KmxBank

// Loading placeholder block with a soft shimmer. Size it like the content
// it stands in for; stack several to sketch a page skeleton.
Rectangle {
    id: root

    implicitWidth: 160
    implicitHeight: 16
    radius: Theme.radiusS
    color: Theme.surfaceAlt

    SequentialAnimation on opacity {
        running: root.visible
        loops: Animation.Infinite
        NumberAnimation { from: 0.45; to: 0.9; duration: 700 }
        NumberAnimation { from: 0.9; to: 0.45; duration: 700 }
    }
}
