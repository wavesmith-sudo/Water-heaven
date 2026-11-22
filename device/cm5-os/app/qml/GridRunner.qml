import QtQuick
import "Theme.js" as T
import "games/RunnerLogic.js" as L

// Grid Runner (original game). Rendered with plain Rectangles so it stays cheap on the
// software scene graph. Depth z: 1 = horizon, 0 = the player's line.
Item {
    id: game
    property real u: 1
    property bool paused: false
    property int score: 0
    property bool over: false
    property int best: 0
    property var st: L.create(1)
    property int frame: 0
    clip: true

    readonly property real cx: width / 2
    readonly property real vanishY: height * 0.16
    readonly property real horizonY: screenY(1.05)
    readonly property real playerY: height * 0.9
    readonly property real laneW: width * 0.15
    function depthScale(z) { return 1 / (1 + Math.max(-0.2, z) * 4) }
    function screenY(z) { return vanishY + (playerY - vanishY) * depthScale(z) }
    function laneX(lane, z) { return cx + (lane - (L.LANES - 1) / 2) * laneW * depthScale(z) }

    function restart() { crashFlash.stop(); flash.opacity = 0; st = L.create(Date.now() & 0x7fffffff); sync() }
    function press(action) { L.input(st, action); sync() }
    function sync() { score = st.score; over = st.over; frame++ }
    Component.onCompleted: restart()

    Timer {
        id: loop
        interval: 16; repeat: true
        running: game.visible && !game.paused && !game.over
        property double last: 0
        onRunningChanged: last = Date.now()
        onTriggered: {
            const now = Date.now()
            const ev = L.step(game.st, (now - last) / 1000)
            last = now
            if (ev.indexOf("crash") >= 0) crashFlash.restart()
            if (ev.indexOf("chip") >= 0) chipPulse.restart()
            game.sync()
        }
    }

    // Sky, sun and floor
    Rectangle { anchors.fill: parent; color: "#07030f" }
    Item {
        width: parent.width; height: game.horizonY; clip: true
        Rectangle { anchors.fill: parent
            gradient: Gradient { GradientStop { position: 0; color: "#0e0522" } GradientStop { position: 1; color: "#4a1260" } } }
        Rectangle {
            width: game.height * 0.36; height: width; radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter; y: parent.height - height * 0.55
            gradient: Gradient { GradientStop { position: 0; color: T.yellow } GradientStop { position: 0.6; color: "#ff6a5c" } GradientStop { position: 1; color: "#e0207f" } }
        }
        Repeater { model: 4
            Rectangle { required property int index; width: game.width; height: (3 + index * 3) * game.u; color: "#3a1058"
                y: game.horizonY - game.height * 0.17 + index * game.height * 0.04 } }
    }
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d"); c.reset()
            c.strokeStyle = "rgba(41, 232, 242, 0.55)"; c.lineWidth = 2
            for (let i = 0; i <= L.LANES; ++i) {
                const off = i - L.LANES / 2
                c.beginPath()
                c.moveTo(game.cx + off * game.laneW * game.depthScale(1.05), game.horizonY)
                c.lineTo(game.cx + off * game.laneW * game.depthScale(-0.2), game.screenY(-0.2))
                c.stroke()
            }
            c.strokeStyle = T.magenta; c.lineWidth = 3
            c.beginPath(); c.moveTo(0, game.horizonY); c.lineTo(width, game.horizonY); c.stroke()
        }
    }
    // Floor stripes scrolling toward the player
    Repeater {
        model: 12
        Rectangle {
            required property int index
            // dist grows 40x faster than objects move, so 0.025 keeps stripes in step with them
            readonly property real depth: { game.frame; return (index + 1) / 12 - (game.st.dist * 0.025) % (1 / 12) }
            readonly property real sc: game.depthScale(depth)
            x: game.cx - game.laneW * L.LANES / 2 * sc
            width: game.laneW * L.LANES * sc
            y: game.screenY(depth); height: Math.max(1, 3 * sc * game.u)
            color: T.magenta; opacity: 0.25 + 0.5 * sc
        }
    }

    // Track objects (a fixed pool; unused slots are hidden)
    Repeater {
        model: 24
        Item {
            id: obj
            required property int index
            readonly property var o: { game.frame; return index < game.st.objs.length ? game.st.objs[index] : null }
            readonly property real sc: o ? game.depthScale(o.z) : 0
            visible: o !== null && !(o.done && o.kind === "chip")
            z: o ? 10 - o.z * 5 : 0
            x: o ? game.laneX(o.lane, o.z) : 0
            y: o ? game.screenY(o.z) : 0
            readonly property real lw: game.laneW * sc
            Rectangle { // low barrier: jump it
                visible: obj.o !== null && obj.o.kind === "barrier"
                width: obj.lw * 0.86; height: obj.lw * 0.2; x: -width / 2; y: -height
                radius: 4 * obj.sc; color: "#3a0f3f"; border.color: T.pink; border.width: Math.max(1, 5 * obj.sc)
                Rectangle { anchors.centerIn: parent; width: parent.width * 0.8; height: Math.max(1, 3 * obj.sc); color: T.pink }
            }
            Rectangle { // tall pylon: dodge it
                visible: obj.o !== null && obj.o.kind === "pylon"
                width: obj.lw * 0.34; height: obj.lw * 0.95; x: -width / 2; y: -height
                radius: 6 * obj.sc
                gradient: Gradient { GradientStop { position: 0; color: T.yellow } GradientStop { position: 1; color: "#ff6a5c" } }
                Rectangle { width: parent.width; height: Math.max(1, 6 * obj.sc); y: parent.height * 0.3; color: "#2b0f4d" }
                Rectangle { width: parent.width; height: Math.max(1, 6 * obj.sc); y: parent.height * 0.55; color: "#2b0f4d" }
            }
            Rectangle { // data chip: collect it
                visible: obj.o !== null && obj.o.kind === "chip"
                width: obj.lw * 0.24; height: width; x: -width / 2; y: -obj.lw * 0.42
                rotation: 45; color: "#103a4a"; border.color: T.cyan; border.width: Math.max(1, 5 * obj.sc)
            }
        }
    }

    // Player craft
    Item {
        id: player
        z: 20
        readonly property real jumpLift: { game.frame; const t = game.st.jump / L.JUMP_TIME; return t > 0 ? Math.sin(Math.PI * (1 - t)) * game.laneW * 0.55 : 0 }
        x: { game.frame; return game.laneX(game.st.lane, 0) }
        Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
        y: game.playerY
        Rectangle { // shadow
            width: game.laneW * 0.62; height: game.laneW * 0.08; x: -width / 2; y: -height / 2
            radius: height / 2; color: "#000000"; opacity: 0.55 - player.jumpLift / game.laneW * 0.5
        }
        Canvas {
            id: craft
            width: game.laneW * 0.66; height: game.laneW * 0.3
            x: -width / 2; y: -height - player.jumpLift
            onWidthChanged: requestPaint()
            onPaint: {
                const c = getContext("2d"), w = width, h = height
                c.reset(); c.lineJoin = "round"
                c.fillStyle = "#13253f"; c.strokeStyle = T.cyan; c.lineWidth = Math.max(2, w * 0.03)
                c.beginPath(); c.moveTo(w * 0.04, h * 0.95); c.lineTo(w * 0.22, h * 0.25); c.lineTo(w * 0.78, h * 0.25); c.lineTo(w * 0.96, h * 0.95); c.closePath(); c.fill(); c.stroke()
                c.fillStyle = T.magenta; c.fillRect(w * 0.34, h * 0.05, w * 0.32, h * 0.22)
                c.fillStyle = T.pink; c.fillRect(w * 0.1, h * 0.66, w * 0.16, h * 0.14); c.fillRect(w * 0.74, h * 0.66, w * 0.16, h * 0.14)
            }
        }
    }

    Rectangle { id: flash; anchors.fill: parent; z: 30; color: T.pink; opacity: 0 }
    SequentialAnimation { id: crashFlash
        NumberAnimation { target: flash; property: "opacity"; to: 0.7; duration: 60 }
        NumberAnimation { target: flash; property: "opacity"; to: 0.2; duration: 500 } }

    // HUD
    GameHud { z: 40; u: game.u; x: 32 * game.u; y: 24 * game.u; label: "SCORE"; value: game.score; color: T.yellow }
    GameHud { z: 40; u: game.u; anchors.right: parent.right; anchors.rightMargin: 32 * game.u; y: 24 * game.u; label: "BEST"; value: Math.max(game.best, game.score) }
    Text {
        id: chipText; z: 40
        anchors.horizontalCenter: parent.horizontalCenter; y: 34 * game.u
        text: { game.frame; return "CHIPS " + game.st.chips }
        color: T.cyan; font.pixelSize: 26 * game.u; font.bold: true; font.letterSpacing: 2
        SequentialAnimation on scale { id: chipPulse; running: false
            NumberAnimation { to: 1.3; duration: 80 } NumberAnimation { to: 1.0; duration: 160 } }
    }
}
