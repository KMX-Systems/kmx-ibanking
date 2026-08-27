import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KmxBank

RowLayout {
    id: root

    property string title
    property string actionText: ""
    signal actionTriggered()

    // Set on the instance a Page uses as its `header:`. The mobile shell already
    // shows the route title in its top bar, so on a compact canvas the page's
    // own title is redundant — but any action button it carries is not, and has
    // nowhere else to go.
    property bool pageHeader: false
    readonly property bool titleVisible: !(pageHeader && FormFactor.compact)

    // Set when the header row holds sibling actions: the bar must keep filling
    // the row so those stay right-aligned once the title is gone.
    property bool reserveSpace: false

    // A Page's header spans the full window, so it needs the page inset itself.
    readonly property int sideMargin: pageHeader ? FormFactor.pageMargin : 0

    spacing: Theme.spacingM

    // An empty header row must not reserve height, or every compact page starts
    // with a band of dead space.
    visible: titleVisible || actionText.length > 0 || reserveSpace

    Label {
        Layout.leftMargin: root.sideMargin
        visible: root.titleVisible
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontSizeH2
        font.bold: true
        Layout.fillWidth: true
    }

    // Keeps the action right-aligned once the title label is gone.
    Item { visible: !root.titleVisible; Layout.fillWidth: true }

    Button {
        Layout.rightMargin: root.sideMargin
        visible: root.actionText.length > 0
        text: root.actionText
        flat: true
        onClicked: root.actionTriggered()
    }
}
