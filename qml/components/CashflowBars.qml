import QtQuick
import KmxBank
import "../theme/Format.js" as Format

// Grouped monthly cashflow bars (income vs expense), RON minor units.
// rows: [{label, income_minor, expense_minor}] oldest first.
Canvas {
    id: root

    property var rows: []
    readonly property color incomeColor: Theme.success
    readonly property color expenseColor: Theme.danger

    implicitWidth: 420
    implicitHeight: 180
    antialiasing: true

    onRowsChanged: if (visible) requestPaint()
    onVisibleChanged: if (visible) requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)

        if (!rows.length)
            return

        let maxV = 1
        for (let i = 0; i < rows.length; ++i) {
            maxV = Math.max(maxV, rows[i].income_minor, rows[i].expense_minor)
        }

        const padL = 8, padB = 20, padT = 8
        const chartH = height - padB - padT
        const slot = (width - padL * 2) / rows.length
        const barW = Math.min(26, slot / 3)

        ctx.font = '10px "' + Theme.fontFamily + '"'
        ctx.textAlign = "center"

        for (let i = 0; i < rows.length; ++i) {
            const r = rows[i]
            const cx = padL + slot * i + slot / 2

            // income bar (left of pair)
            const hIn = chartH * (r.income_minor / maxV)
            ctx.fillStyle = String(incomeColor)
            ctx.fillRect(cx - barW - 2, height - padB - hIn, barW, hIn)

            // expense bar (right of pair)
            const hOut = chartH * (r.expense_minor / maxV)
            ctx.fillStyle = String(expenseColor)
            ctx.fillRect(cx + 2, height - padB - hOut, barW, hOut)

            // month label
            ctx.fillStyle = String(Theme.textMuted)
            ctx.fillText(r.label, cx, height - 6)

            // value captions on hover-ish: always show compact for last row
            if (i === rows.length - 1) {
                ctx.fillStyle = String(incomeColor)
                ctx.fillText((r.income_minor / 100).toFixed(0),
                             cx - barW / 2 - 2, height - padB - hIn - 4)
                ctx.fillStyle = String(expenseColor)
                ctx.fillText((r.expense_minor / 100).toFixed(0),
                             cx + barW / 2 + 2, height - padB - hOut - 4)
            }
        }

        // baseline
        ctx.strokeStyle = String(Theme.border)
        ctx.beginPath()
        ctx.moveTo(0, height - padB + 0.5)
        ctx.lineTo(width, height - padB + 0.5)
        ctx.stroke()
    }
}
