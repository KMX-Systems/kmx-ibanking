pragma Singleton
import QtQuick

// Layout state shared by both shells: which size class the window currently
// falls into, and the metrics that differ between pointer and touch input.
//
// UiConfig (C++) decides *which* shell starts; FormFactor describes the canvas
// that shell ended up with. Pages ask `FormFactor.compact`, never the window
// width, so a narrow desktop window and a handset reflow the same way.
QtObject {
    id: root

    // The active shell writes these; everything below derives from them.
    property real windowWidth: 0
    property real windowHeight: 0

    // True while the mobile shell is driving, regardless of window size. Use it
    // for input affordances (hit targets, swipe-back); use `compact` for layout.
    property bool mobileShell: false

    // ---- size classes (Material 3 window-size classes, logical px) ---------
    readonly property int compactMaxWidth: 599
    readonly property int mediumMaxWidth: 904

    readonly property string tier:
        windowWidth <= compactMaxWidth ? "compact"
      : windowWidth <= mediumMaxWidth  ? "medium"
                                       : "expanded"

    readonly property bool compact:  tier === "compact"
    readonly property bool medium:   tier === "medium"
    readonly property bool expanded: tier === "expanded"

    // Short viewports (phone landscape) cannot afford tall heroes and headers.
    readonly property bool shortViewport: windowHeight > 0 && windowHeight < 520

    // ---- metrics -----------------------------------------------------------
    // Fingers need ~48dp; a mouse is happy with far less.
    readonly property int touchTarget: mobileShell ? 48 : 32
    readonly property int iconSize: mobileShell ? 22 : 20

    readonly property int pageMargin: compact ? 12 : 16
    readonly property int cardSpacing: compact ? 10 : 16

    readonly property int topBarHeight: 56
    readonly property int navBarHeight: 64

    // How many equal-width cards fit in a row of chips/tiles.
    readonly property int tileColumns: compact ? 2 : medium ? 3 : 4
}
