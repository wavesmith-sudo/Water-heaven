import QtQuick
import "Theme.js" as T

// Synthwave sunset, skyline and perspective grid. Drawn with Canvas so it works on the
// software scene-graph backend the CM5 launcher currently uses; repaints only on resize.
Item {
    id: root
    property real horizon: height * 0.52
    property bool dim: false

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#030c1c" }
            GradientStop { position: 0.45; color: "#0b2a55" }
            GradientStop { position: 0.52; color: "#135a8f" }
            GradientStop { position: 0.53; color: "#061633" }
            GradientStop { position: 1.0; color: "#020812" }
        }
    }
    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d"); var w = width, h = height, hz = root.horizon
            ctx.reset()
            // Sun with horizontal cut-outs
            var r = Math.min(w, h) * 0.2, cx = w / 2, cy = hz - r * 0.15
            var g = ctx.createLinearGradient(0, cy - r, 0, cy + r)
            g.addColorStop(0, "#9ef0ff"); g.addColorStop(0.55, "#3d9bff"); g.addColorStop(1, "#1f4fd1")
            ctx.save(); ctx.beginPath(); ctx.rect(0, 0, w, hz); ctx.clip()
            ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill()
            ctx.fillStyle = "#0b2a55"
            for (var i = 0; i < 6; ++i) {
                var y = cy - r * (0.62 - i * 0.14), bh = r * (0.025 + i * 0.016)
                ctx.fillRect(cx - r, y, r * 2, bh)
            }
            // Skyline silhouettes
            ctx.fillStyle = "#071a36"
            var x = 0, seed = 7
            while (x < w) {
                seed = (seed * 9301 + 49297) % 233280
                var bw = w * (0.025 + (seed / 233280) * 0.035)
                seed = (seed * 9301 + 49297) % 233280
                var bhh = h * (0.04 + (seed / 233280) * 0.12)
                if (Math.abs(x + bw / 2 - cx) < r * 0.9) bhh *= 0.45
                ctx.fillRect(x, hz - bhh, bw - 2, bhh)
                x += bw
            }
            ctx.restore()
            // Perspective grid floor
            ctx.strokeStyle = "rgba(61, 155, 255, 0.55)"; ctx.lineWidth = 1.5
            for (var k = -14; k <= 14; ++k) {
                ctx.beginPath(); ctx.moveTo(cx + k * w * 0.012, hz); ctx.lineTo(cx + k * w * 0.16, h); ctx.stroke()
            }
            for (var j = 1; j < 14; ++j) {
                var t = j / 14, yy = hz + (h - hz) * t * t
                ctx.beginPath(); ctx.moveTo(0, yy); ctx.lineTo(w, yy); ctx.stroke()
            }
            ctx.strokeStyle = T.magenta; ctx.lineWidth = 2
            ctx.beginPath(); ctx.moveTo(0, hz); ctx.lineTo(w, hz); ctx.stroke()
        }
    }
    Rectangle { anchors.fill: parent; color: "#000000"; opacity: root.dim ? 0.78 : 0; Behavior on opacity { NumberAnimation { duration: 200 } } }
}
