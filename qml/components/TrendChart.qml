import QtQuick
import KmxBank
import "../theme/Format.js" as Format

// Net-worth trend line with hover tooltip (plan §Phase 9).
// rows: [{label, total_minor}] oldest first, RON minor units.
Canvas {
    id: root

    property var rows: []
    property int hoverIndex: -1
    readonly property color lineColor: Theme.accent

    implicitWidth: 480
    implicitHeight: 180
    antialiasing: true

    onRowsChanged: if (visible) { hoverIndex = -1; requestPaint() }
    onVisibleChanged: if (visible) requestPaint()
    onHoverIndexChanged: if (visible) requestPaint()

    function _points() {
        const n = rows.length
        if (n < 2)
            return null
        let min = Infinity, max = -Infinity
        for (let i = 0; i < n; ++i) {
            min = Math.min(min, rows[i].total_minor)
            max = Math.max(max, rows[i].total_minor)
        }
        if (max - min < 1)
            max = min + 1
        const padL = 10, padR = 10, padT = 14, padB = 20
        const w = width - padL - padR, h = height - padT - padB
        const pts = []
        for (let i = 0; i < n; ++i) {
            pts.push({
                x: padL + w * i / (n - 1),
                y: padT + h * (1 - (rows[i].total_minor - min) / (max - min)),
                v: rows[i].total_minor,
                label: rows[i].label
            })
        }
        return pts
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)

        const pts = _points()
        if (!pts)
            return

        // area fill
        ctx.beginPath()
        ctx.moveTo(pts[0].x, height - 20)
        for (let i = 0; i < pts.length; ++i)
            ctx.lineTo(pts[i].x, pts[i].y)
        ctx.lineTo(pts[pts.length - 1].x, height - 20)
        ctx.closePath()
        ctx.fillStyle = Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.12)
        ctx.fill()

        // line
        ctx.beginPath()
        for (let i2 = 0; i2 < pts.length; ++i2) {
            if (i2 === 0)
                ctx.moveTo(pts[i2].x, pts[i2].y)
            else
                ctx.lineTo(pts[i2].x, pts[i2].y)
        }
        ctx.strokeStyle = String(lineColor)
        ctx.lineWidth = 2.4
        ctx.lineJoin = "round"
        ctx.stroke()

        // month labels + markers
        ctx.font = '10px "' + Theme.fontFamily + '"'
        ctx.textAlign = "center"
        for (let i3 = 0; i3 < pts.length; ++i3) {
            ctx.fillStyle = String(Theme.textMuted)
            ctx.fillText(pts[i3].label, pts[i3].x, height - 6)

            ctx.beginPath()
            ctx.arc(pts[i3].x, pts[i3].y, i3 === hoverIndex ? 5 : 3, 0, Math.PI * 2)
            ctx.fillStyle = String(i3 === hoverIndex ? Theme.text : lineColor)
            ctx.fill()
        }

        // tooltip
        if (hoverIndex >= 0 && hoverIndex < pts.length) {
            const p = pts[hoverIndex]
            const text = Format.money(p.v, "RON") + " · " + p.label
            ctx.font = '11px "' + Theme.fontFamily + '"'
            const tw = ctx.measureText(text).width + 16
            let bx = Math.max(4, Math.min(width - tw - 4, p.x - tw / 2))
            const by = Math.max(4, p.y - 34)

            ctx.fillStyle = String(Theme.surface)
            ctx.strokeStyle = String(Theme.border)
            ctx.beginPath()
            ctx.roundRect ? ctx.roundRect(bx, by, tw, 24, 6)
                          : ctx.rect(bx, by, tw, 24)
            ctx.fill()
            ctx.stroke()

            ctx.fillStyle = String(Theme.text)
            ctx.textAlign = "left"
            ctx.fillText(text, bx + 8, by + 16)
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: function (mouse) {
            const pts = root._points()
            if (!pts)
                return
            let nearest = -1, bestD = Infinity
            for (let i = 0; i < pts.length; ++i) {
                const d = Math.abs(pts[i].x - mouse.x)
                if (d < bestD) { bestD = d; nearest = i }
            }
            if (nearest !== root.hoverIndex) {
                root.hoverIndex = nearest
                root.requestPaint()
            }
        }
        onExited: {
            root.hoverIndex = -1
            root.requestPaint()
        }
    }
}
