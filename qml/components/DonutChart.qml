import QtQuick
import KmxBank
import "../theme/Format.js" as Format

// Spending donut (plan §Phase 9): segments = [{category, spent_minor}] in RON.
// Center shows the month total; tap/hover selects the biggest slice under
// the pointer for the caption. Visibility-gated repaints per plan rule 5.
Canvas {
    id: root

    property var segments: []              // [{category, spent_minor}]
    property int hoveredCategory: -1

    readonly property real total:
        segments.reduce((acc, s) => acc + s.spent_minor, 0)

    implicitWidth: 220
    implicitHeight: 220
    antialiasing: true

    onSegmentsChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onWidthChanged: if (visible) requestPaint()
    onHeightChanged: if (visible) requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)

        const cx = width / 2, cy = height / 2
        const outer = Math.min(width, height) / 2 - 6
        const inner = outer * 0.58

        if (!segments.length || total <= 0) {
            ctx.beginPath()
            ctx.arc(cx, cy, inner + (outer - inner) / 2, 0, Math.PI * 2)
            ctx.strokeStyle = Theme.surfaceAlt
            ctx.lineWidth = outer - inner
            ctx.stroke()
            return
        }

        let startAngle = -Math.PI / 2
        for (let i = 0; i < segments.length; ++i) {
            const seg = segments[i]
            const sweep = (seg.spent_minor / total) * Math.PI * 2
            const isHot = seg.category === hoveredCategory

            ctx.beginPath()
            ctx.arc(cx, cy, outer, startAngle, startAngle + sweep)
            ctx.arc(cx, cy, inner, startAngle + sweep, startAngle, true)
            ctx.closePath()

            const col = Theme.categoryColors[seg.category]
            ctx.fillStyle = Qt.rgba(col.r, col.g, col.b,
                                    isHot ? 1.0 : 0.85)
            ctx.fill()

            if (isHot || segments.length === 1) {
                ctx.strokeStyle = Theme.text
                ctx.lineWidth = 2
                ctx.stroke()
            }
            startAngle += sweep
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: function (mouse) {
            const dx = mouse.x - width / 2
            const dy = mouse.y - height / 2
            const dist = Math.sqrt(dx * dx + dy * dy)
            const outer = Math.min(width, height) / 2 - 6
            const inner = outer * 0.58

            if (dist < inner || dist > outer) {
                root.hoveredCategory = -1
                return
            }
            let ang = Math.atan2(dy, dx) + Math.PI / 2
            if (ang < 0)
                ang += Math.PI * 2

            let acc = 0
            root.hoveredCategory = -1
            for (let i = 0; i < root.segments.length; ++i) {
                acc += (root.segments[i].spent_minor / root.total) * Math.PI * 2
                if (ang <= acc) {
                    root.hoveredCategory = root.segments[i].category
                    break
                }
            }
            root.requestPaint()
        }
        onExited: {
            root.hoveredCategory = -1
            root.requestPaint()
        }
    }

    // Center captions bound from the page.
    Text {
        anchors.centerIn: parent
        text: Format.money(root.total, "RON")
        color: Theme.text
        font: Theme.amountFont(Theme.fontSizeH3)
    }
}
