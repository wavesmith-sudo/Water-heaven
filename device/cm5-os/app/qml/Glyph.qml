import QtQuick

// Simple line icons drawn on Canvas, so the image needs no icon font or emoji support.
Canvas {
    id: c
    property string kind: "movies"
    property color color: "white"
    onKindChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onPaint: {
        var x = getContext("2d"), s = width
        x.reset(); x.strokeStyle = color; x.fillStyle = color
        x.lineWidth = Math.max(2, s * 0.07); x.lineJoin = "round"; x.lineCap = "round"
        function rr(a, b, w, h, r) { x.beginPath(); x.moveTo(a + r, b); x.lineTo(a + w - r, b); x.quadraticCurveTo(a + w, b, a + w, b + r); x.lineTo(a + w, b + h - r); x.quadraticCurveTo(a + w, b + h, a + w - r, b + h); x.lineTo(a + r, b + h); x.quadraticCurveTo(a, b + h, a, b + h - r); x.lineTo(a, b + r); x.quadraticCurveTo(a, b, a + r, b); x.closePath() }
        if (kind === "movies") {
            rr(s*.12, s*.38, s*.76, s*.5, s*.06); x.stroke()
            x.beginPath(); x.moveTo(s*.12, s*.38); x.lineTo(s*.82, s*.16); x.lineTo(s*.86, s*.3); x.lineTo(s*.16, s*.52); x.closePath(); x.stroke()
            for (var i = 0; i < 3; ++i) { x.beginPath(); x.moveTo(s*(.3+i*.18), s*.32 - i*s*.055); x.lineTo(s*(.38+i*.18), s*.44 - i*s*.055); x.stroke() }
        } else if (kind === "tv") {
            rr(s*.1, s*.28, s*.8, s*.52, s*.06); x.stroke()
            x.beginPath(); x.moveTo(s*.36, s*.1); x.lineTo(s*.5, s*.26); x.lineTo(s*.64, s*.1); x.stroke()
            x.beginPath(); x.moveTo(s*.3, s*.9); x.lineTo(s*.7, s*.9); x.stroke()
        } else if (kind === "music") {
            x.beginPath(); x.moveTo(s*.38, s*.72); x.lineTo(s*.38, s*.18); x.lineTo(s*.8, s*.1); x.lineTo(s*.8, s*.64); x.stroke()
            x.beginPath(); x.ellipse(s*.16, s*.64, s*.24, s*.18); x.fill()
            x.beginPath(); x.ellipse(s*.58, s*.56, s*.24, s*.18); x.fill()
        } else if (kind === "games") {
            rr(s*.08, s*.3, s*.84, s*.44, s*.2); x.stroke()
            x.beginPath(); x.moveTo(s*.27, s*.42); x.lineTo(s*.27, s*.62); x.moveTo(s*.17, s*.52); x.lineTo(s*.37, s*.52); x.stroke()
            x.beginPath(); x.arc(s*.66, s*.46, s*.045, 0, 7); x.fill(); x.beginPath(); x.arc(s*.76, s*.58, s*.045, 0, 7); x.fill()
        } else if (kind === "settings") {
            x.beginPath(); x.arc(s*.5, s*.5, s*.13, 0, 7); x.stroke()
            for (var k = 0; k < 8; ++k) { var a = k * Math.PI / 4; x.beginPath(); x.moveTo(s*.5 + Math.cos(a)*s*.25, s*.5 + Math.sin(a)*s*.25); x.lineTo(s*.5 + Math.cos(a)*s*.38, s*.5 + Math.sin(a)*s*.38); x.stroke() }
            x.beginPath(); x.arc(s*.5, s*.5, s*.27, 0, 7); x.stroke()
        } else if (kind === "resume") {
            x.beginPath(); x.arc(s*.5, s*.5, s*.38, 0, 7); x.stroke()
            x.beginPath(); x.moveTo(s*.42, s*.34); x.lineTo(s*.66, s*.5); x.lineTo(s*.42, s*.66); x.closePath(); x.fill()
        }
    }
}
