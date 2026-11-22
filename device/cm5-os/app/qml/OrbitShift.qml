import QtQuick
import "Theme.js" as T
import "games/OrbitLogic.js" as L

// Orbit Shift (original game). Sparks leave the core along sixteen spokes; meet the ones
// that match your colour on the ring and dodge the others.
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

    readonly property real ringR: height * 0.4
    readonly property color playerColor: { frame; return st.polarity === 0 ? T.cyan : T.magenta }
    function slotAngle(slot) { return slot * 360 / L.SLOTS - 90 }

    function restart() { st = L.create(Date.now() & 0x7fffffff); hitFlash.stop(); flash.opacity = 0; sync() }
    function press(action) { L.input(st, action); sync() }
    function sync() { score = st.score; over = st.over; frame++ }
    Component.onCompleted: restart()

    Timer {
        interval: 16; repeat: true
        running: game.visible && !game.paused && !game.over
        property double last: 0
        onRunningChanged: last = Date.now()
        onTriggered: {
            const now = Date.now()
            const ev = L.step(game.st, (now - last) / 1000)
            last = now
            if (ev.indexOf("hit") >= 0) hitFlash.restart()
            if (ev.indexOf("catch") >= 0) catchPulse.restart()
            game.sync()
        }
    }

    Rectangle { anchors.fill: parent; color: "#08041a" }
    Item {
        id: arena
        width: game.ringR * 2.5; height: width
        anchors.centerIn: parent

        Canvas { // spokes and ring guide
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onPaint: {
                const c = getContext("2d"), m = width / 2, R = game.ringR
                c.reset()
                c.strokeStyle = "rgba(185, 173, 214, 0.18)"; c.lineWidth = 2
                for (let i = 0; i < L.SLOTS; ++i) {
                    const a = game.slotAngle(i) * Math.PI / 180
                    c.beginPath(); c.moveTo(m + Math.cos(a) * R * 0.16, m + Math.sin(a) * R * 0.16)
                    c.lineTo(m + Math.cos(a) * R * 1.12, m + Math.sin(a) * R * 1.12); c.stroke()
                }
                c.strokeStyle = "rgba(233, 91, 255, 0.5)"; c.lineWidth = 3
                c.beginPath(); c.arc(m, m, R, 0, Math.PI * 2); c.stroke()
                c.strokeStyle = "rgba(41, 232, 242, 0.25)"; c.lineWidth = 1.5
                c.beginPath(); c.arc(m, m, R * 1.12, 0, Math.PI * 2); c.stroke()
            }
        }
        Rectangle { // core
            id: core
            width: game.ringR * 0.26; height: width; radius: width / 2
            anchors.centerIn: parent
            color: "#2b0f4d"; border.color: T.yellow; border.width: 4 * game.u
            Rectangle { anchors.centerIn: parent; width: parent.width * 0.45; height: width; radius: width / 2; color: T.yellow
                SequentialAnimation on opacity { loops: Animation.Infinite
                    NumberAnimation { to: 0.4; duration: 700 } NumberAnimation { to: 1; duration: 700 } } }
        }
        Repeater {
            model: 40
            Rectangle {
                required property int index
                readonly property var p: { game.frame; return index < game.st.sparks.length ? game.st.sparks[index] : null }
                readonly property real a: p ? game.slotAngle(p.slot) * Math.PI / 180 : 0
                readonly property real dist: p ? game.ringR * (0.13 + 0.87 * p.r) : 0
                visible: p !== null
                width: game.ringR * (0.06 + 0.06 * (p ? p.r : 0)); height: width; radius: width / 2
                x: arena.width / 2 + Math.cos(a) * dist - width / 2
                y: arena.height / 2 + Math.sin(a) * dist - height / 2
                color: p && p.pol === 0 ? T.cyan : T.magenta
                border.color: "white"; border.width: Math.max(1, width * 0.12)
            }
        }
        Item { // the player's shield, riding the ring
            id: shield
            anchors.fill: parent
            rotation: { game.frame; return game.slotAngle(game.st.slot) + 90 }
            Behavior on rotation { RotationAnimation { duration: 90; direction: RotationAnimation.Shortest } }
            Rectangle {
                width: game.ringR * 0.42; height: 18 * game.u; radius: height / 2
                x: parent.width / 2 - width / 2
                y: parent.height / 2 - game.ringR - height / 2
                color: game.playerColor
                border.color: "white"; border.width: 3 * game.u
                opacity: { game.frame; return game.st.guard > 0 ? 0.4 + 0.6 * Math.abs(Math.sin(game.st.guard * 14)) : 1 }
            }
        }
    }

    Rectangle { id: flash; anchors.fill: parent; color: T.pink; opacity: 0 }
    SequentialAnimation { id: hitFlash
        NumberAnimation { target: flash; property: "opacity"; to: 0.55; duration: 60 }
        NumberAnimation { target: flash; property: "opacity"; to: 0; duration: 450 } }

    GameHud { u: game.u; x: 32 * game.u; y: 24 * game.u; label: "SCORE"; value: game.score; color: T.yellow }
    GameHud { u: game.u; anchors.right: parent.right; anchors.rightMargin: 32 * game.u; y: 24 * game.u; label: "BEST"; value: Math.max(game.best, game.score) }
    Column {
        x: 32 * game.u; anchors.bottom: parent.bottom; anchors.bottomMargin: 28 * game.u; spacing: 10 * game.u
        Text { text: "SHIELD"; color: T.muted; font.pixelSize: 18 * game.u; font.bold: true; font.letterSpacing: 2 }
        Row { spacing: 12 * game.u
            Rectangle { width: 34 * game.u; height: width; radius: width / 2; color: game.playerColor; border.color: "white"; border.width: 3 * game.u }
            Text { anchors.verticalCenter: parent.verticalCenter; text: { game.frame; return game.st.polarity === 0 ? "CYAN" : "MAGENTA" }
                color: game.playerColor; font.pixelSize: 26 * game.u; font.bold: true }
        }
    }
    Column {
        anchors.right: parent.right; anchors.rightMargin: 32 * game.u
        anchors.bottom: parent.bottom; anchors.bottomMargin: 28 * game.u; spacing: 10 * game.u
        Text { anchors.right: parent.right; text: "LIVES"; color: T.muted; font.pixelSize: 18 * game.u; font.bold: true; font.letterSpacing: 2 }
        Row { anchors.right: parent.right; spacing: 12 * game.u
            Repeater { model: L.LIVES
                Rectangle { required property int index
                    width: 26 * game.u; height: width; rotation: 45
                    color: { game.frame; return index < game.st.lives ? T.pink : "transparent" }
                    border.color: T.pink; border.width: 3 * game.u } }
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; y: 34 * game.u
        text: { game.frame; return game.st.combo > 1 ? "COMBO ×" + Math.min(8, game.st.combo) : "" }
        color: T.cyan; font.pixelSize: 28 * game.u; font.bold: true; font.letterSpacing: 2
        SequentialAnimation on scale { id: catchPulse; running: false
            NumberAnimation { to: 1.25; duration: 80 } NumberAnimation { to: 1.0; duration: 160 } }
    }
}
