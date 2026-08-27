import QtQuick
import KmxBank

// Tiny inline line chart for account/FX history. Values are normalized to
// [0,1]; repaints only on data/size change (visibility-gated per plan rule 5).
Canvas {
    id: root

    property var values: []
    property color lineColor: Theme.accent
    property bool fillUnder: true

    implicitWidth: 120
    implicitHeight: 36
    antialiasing: true

    onValuesChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function _points() {
        var n = values ? values.length : 0
        if (n < 2)
            return null
        var min = Infinity, max = -Infinity
        for (var i = 0; i < n; ++i) {
            min = Math.min(min, values[i])
            max = Math.max(max, values[i])
        }
        if (max - min < 1e-9)
            max = min + 1
        var pts = []
        for (i = 0; i < n; ++i) {
            pts.push({
                x: width * 0.04 + (width * 0.92) * i / (n - 1),
                y: height * 0.88 - height * 0.76 * (values[i] - min) / (max - min)
            })
        }
        return pts
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var pts = _points()
        if (!pts)
            return

        ctx.lineJoin = "round"
        ctx.lineCap = "round"

        if (fillUnder) {
            ctx.beginPath()
            ctx.moveTo(pts[0].x, height)
            for (var i = 0; i < pts.length; ++i)
                ctx.lineTo(pts[i].x, pts[i].y)
            ctx.lineTo(pts[pts.length - 1].x, height)
            ctx.closePath()
            ctx.fillStyle = Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.14)
            ctx.fill()
        }

        ctx.beginPath()
        for (i = 0; i < pts.length; ++i) {
            if (i === 0)
                ctx.moveTo(pts[i].x, pts[i].y)
            else
                ctx.lineTo(pts[i].x, pts[i].y)
        }
        ctx.strokeStyle = String(lineColor)
        ctx.lineWidth = 2
        ctx.stroke()

        ctx.beginPath()
        ctx.arc(pts[pts.length - 1].x, pts[pts.length - 1].y, 2.6, 0, Math.PI * 2)
        ctx.fillStyle = String(lineColor)
        ctx.fill()
    }
}
