import QtQuick
import "Theme.js" as T
import "games/BeamLogic.js" as L

// Beam Router (original puzzle). Turn mirror tiles until the laser runs from the emitter
// on the left to the receiver on the right. Boards are generated, so every one is new.
Item {
    id: game
    property real u: 1
    property bool paused: false
    property int score: 0
    property bool over: false       // the puzzle has no fail state; quit with Back
    property int best: 0
    property var lv: L.createLevel(1, 1)
    property var beam: L.trace(lv)
    property int col: 0
    property int row: 0
    property int frame: 0
    property bool solved: false
    clip: true

    readonly property real tile: Math.min(width * 0.78 / L.COLS, height * 0.74 / L.ROWS)

    function startLevel(n) {
        lv = L.createLevel(n, (Date.now() + n * 7919) & 0x7fffffff)
        col = 0; row = lv.startRow; solved = false
        refresh()
    }
    function restart() { score = 0; startLevel(1) }
    function refresh() { beam = L.trace(lv); frame++ }
    function press(action) {
        if (solved) return
        if (action === "left") col = Math.max(0, col - 1)
        else if (action === "right") col = Math.min(L.COLS - 1, col + 1)
        else if (action === "up") row = Math.max(0, row - 1)
        else if (action === "down") row = Math.min(L.ROWS - 1, row + 1)
        else if (action === "ok" && L.turn(lv, col, row)) {
            refresh()
            if (beam.solved) { solved = true; gained = L.levelScore(lv); score += gained; next.restart() }
        }
    }
    property int gained: 0
    Component.onCompleted: restart()
    Timer { id: next; interval: 1600; onTriggered: game.startLevel(game.lv.level + 1) }

    Rectangle { anchors.fill: parent; color: "#031024" }

    Item {
        id: board
        width: game.tile * L.COLS; height: game.tile * L.ROWS
        anchors.centerIn: parent; anchors.verticalCenterOffset: 30 * game.u

        Repeater {
            model: L.COLS * L.ROWS
            Item {
                id: cell
                required property int index
                readonly property int cx: index % L.COLS
                readonly property int cy: Math.floor(index / L.COLS)
                readonly property int mask: { game.frame; return game.lv.tiles[index].mask }
                readonly property bool lit: { game.frame; return game.beam.lit.indexOf(index) >= 0 }
                readonly property bool cursor: cx === game.col && cy === game.row && !game.solved
                x: cx * game.tile; y: cy * game.tile
                width: game.tile; height: game.tile
                Rectangle {
                    anchors.fill: parent; anchors.margins: 4 * game.u; radius: 10 * game.u
                    color: cell.cursor ? "#12345e" : "#081a35"
                    border.color: cell.cursor ? T.yellow : "#1f4a7a"; border.width: cell.cursor ? 5 * game.u : 2 * game.u
                }
                Canvas {
                    id: pipe
                    anchors.fill: parent
                    property int mask: cell.mask
                    property bool lit: cell.lit
                    onMaskChanged: requestPaint(); onLitChanged: requestPaint(); onWidthChanged: requestPaint()
                    onPaint: {
                        const c = getContext("2d"), s = width, m = s / 2
                        c.reset()
                        if (!mask) return
                        c.lineCap = "butt"
                        const draw = (color, w) => {
                            c.strokeStyle = color; c.lineWidth = w
                            const ends = [[m, 0], [s, m], [m, s], [0, m]]
                            for (let b = 0; b < 4; ++b) if (mask & (1 << b)) { c.beginPath(); c.moveTo(m, m); c.lineTo(ends[b][0], ends[b][1]); c.stroke() }
                        }
                        if (lit) { draw("rgba(41, 232, 242, 0.35)", s * 0.22); draw("#e9fdff", s * 0.07) }
                        else draw("#3e6a99", s * 0.12)
                        c.fillStyle = lit ? T.cyan : "#5f8fc0"
                        c.beginPath(); c.arc(m, m, s * 0.1, 0, Math.PI * 2); c.fill()
                    }
                }
                SequentialAnimation on scale { id: turnPulse; running: false
                    NumberAnimation { to: 0.9; duration: 50 } NumberAnimation { to: 1.0; duration: 120 } }
                onMaskChanged: turnPulse.restart()
            }
        }
        // Emitter and receiver
        Rectangle { // the beam leaving the emitter
            width: 10 * game.u; height: 8 * game.u; x: -width; y: (game.lv.startRow + 0.5) * game.tile - height / 2
            color: "#e9fdff" }
        Canvas {
            width: game.tile * 0.5; height: game.tile * 0.6
            x: -width - 6 * game.u; y: (game.lv.startRow + 0.5) * game.tile - height / 2
            onWidthChanged: requestPaint()
            onPaint: { const c = getContext("2d"); c.reset(); c.fillStyle = T.magenta
                c.beginPath(); c.moveTo(0, 0); c.lineTo(width, height / 2); c.lineTo(0, height); c.closePath(); c.fill() }
        }
        Rectangle {
            width: game.tile * 0.46; height: width; radius: width / 2
            x: board.width + 10 * game.u; y: (game.lv.endRow + 0.5) * game.tile - height / 2
            color: game.solved ? T.cyan : "transparent"; border.color: T.cyan; border.width: 5 * game.u
        }
    }

    GameHud { u: game.u; x: 32 * game.u; y: 24 * game.u; label: "SCORE"; value: game.score; color: T.yellow }
    GameHud { u: game.u; anchors.horizontalCenter: parent.horizontalCenter; y: 24 * game.u; label: "BOARD"
        value: { game.frame; return game.lv.level } }
    GameHud { u: game.u; anchors.right: parent.right; anchors.rightMargin: 32 * game.u; y: 24 * game.u; label: "TURNS / PAR"
        value: { game.frame; return game.lv.moves + " / " + game.lv.par } }

    Rectangle {
        visible: game.solved
        anchors.centerIn: board
        width: 640 * game.u; height: 150 * game.u; radius: 16 * game.u
        color: "#0a1d3a"; border.color: T.cyan; border.width: 4 * game.u
        Column { anchors.centerIn: parent; spacing: 6 * game.u
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "BEAM LOCKED"; color: T.cyan; font.pixelSize: 48 * game.u; font.bold: true; font.letterSpacing: 4 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "+" + game.gained + "   next board…"; color: T.yellow; font.pixelSize: 26 * game.u; font.bold: true }
        }
    }
}
