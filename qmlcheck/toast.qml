import QtQuick
import QtQuick.Controls
import KmxBank

ApplicationWindow {
    id: win
    width: 700; height: 500; visible: true
    color: Theme.bg

    ToastHost { id: toasts; anchors.fill: parent }

    // Reach into the Repeater's delegates the way a tap would.
    function tapNewest() {
        // ToastHost's only visual child is the Column holding the toast cards.
        const stack = toasts.children[0]
        if (!stack) { console.log("no stack"); return }
        for (let i = 0; i < stack.children.length; ++i) {
            const c = stack.children[i]
            if (c && c.opacity === 1) {
                console.log("tapping toast, siblings=" + stack.children.length)
                c.opacity = 0   // fade -> Behavior onFinished -> _dismiss -> model rebuild
                return
            }
        }
        console.log("no toast to tap (" + stack.children.length + " children)")
    }

    property int tick: 0
    Timer {
        interval: 250; running: true; repeat: true
        onTriggered: {
            win.tick++
            if (win.tick % 3 === 1)
                toasts.notify("info", "Toast #" + win.tick, "Undo", function () {})
            else
                win.tapNewest()
            if (win.tick > 40) { console.log("SURVIVED"); Qt.quit() }
        }
    }
}
