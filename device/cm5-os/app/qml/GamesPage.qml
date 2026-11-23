import QtQuick
import "Theme.js" as T

// Retro Games: a picker of the built-in original games, and the running game itself.
// Everything is playable with a TV remote: D-pad, OK and Back.
Item {
    id: page
    property real u: 1
    property int selected: 0
    property string playing: ""          // id of the running game, "" on the picker
    property bool paused: false
    property bool newBest: false
    property int scoresVersion: 0         // bumps when a best score changes, for bindings below
    Connections { target: scores; function onChanged() { page.scoresVersion++ } }
    // Leaving the page (Home key) pauses a running game rather than losing it.
    onVisibleChanged: if (!visible && playing && game.item && !game.item.over) paused = true
    function bestFor(id) { scoresVersion; return scores.best(id) }

    readonly property var games: [
        { id: "runner", title: "Grid Runner", tag: "Dodge and dash", glyph: "runner", color: T.cyan, file: "GridRunner.qml",
          blurb: "Race down five neon lanes. Hop the low barriers, swerve round the pylons and grab the cyan chips.",
          controls: "◀ ▶ steer    OK jump", keys: [{key: "◀ ▶", label: "Steer"}, {key: "A", label: "Jump", color: T.ok}] },
        { id: "orbit", title: "Orbit Shift", tag: "Catch the colour", glyph: "orbit", color: T.magenta, file: "OrbitShift.qml",
          blurb: "Sparks burst from the core. Slide your shield round the ring to catch your colour and dodge the rest.",
          controls: "◀ ▶ orbit    OK swap colour", keys: [{key: "◀ ▶", label: "Orbit"}, {key: "A", label: "Swap colour", color: T.ok}] },
        { id: "beam", title: "Beam Router", tag: "Bend the light", glyph: "beam", color: T.yellow, file: "BeamRouter.qml",
          blurb: "Turn the mirror tiles until the laser reaches the receiver. A new board every time.",
          controls: "▲ ▼ ◀ ▶ move    OK turn tile", keys: [{key: "▲ ▼ ◀ ▶", label: "Move"}, {key: "A", label: "Turn tile", color: T.ok}] }
    ]
    readonly property var current: playing ? games.find(g => g.id === playing) : null
    readonly property string title: current ? current.title : ""
    readonly property var hints: !playing
        ? [{key: "◀ ▶", label: "Choose"}, {key: "A", label: "Play", color: T.ok}, {key: "B", label: "Back", color: "#e0303c"}]
        : (game.item && game.item.over) || paused
        ? [{key: "A", label: paused ? "Resume" : "Play again", color: T.ok}, {key: "B", label: "Quit", color: "#e0303c"}]
        : current.keys.concat([{key: "B", label: "Pause", color: "#e0303c"}])

    function start(i) {
        selected = i
        paused = false; newBest = false
        playing = games[i].id
    }
    function finish() {
        if (game.item) newBest = scores.submit(playing, game.item.score)
    }
    function quit() {
        if (game.item && !game.item.over) finish()
        playing = ""; paused = false
    }

    // Returns true when the key was used here; Back on the picker is left to the caller.
    function handleKey(key) {
        const action = key === Qt.Key_Left ? "left" : key === Qt.Key_Right ? "right"
                     : key === Qt.Key_Up ? "up" : key === Qt.Key_Down ? "down"
                     : (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) ? "ok"
                     : (key === Qt.Key_Escape || key === Qt.Key_Backspace || key === Qt.Key_Back) ? "back"
                     : (key === Qt.Key_MediaTogglePlayPause || key === Qt.Key_MediaPause || key === Qt.Key_MediaPlay) ? "pause" : ""
        if (!action) return false
        if (!playing) {
            if (action === "left") selected = Math.max(0, selected - 1)
            else if (action === "right") selected = Math.min(games.length - 1, selected + 1)
            else if (action === "ok") start(selected)
            else return false
            return true
        }
        const g = game.item
        if (!g) return true
        if (g.over) {
            if (action === "ok") { newBest = false; g.restart() }
            else if (action === "back") quit()
        } else if (paused) {
            if (action === "ok" || action === "pause") paused = false
            else if (action === "back") quit()
        } else if (action === "back" || action === "pause") {
            paused = true
        } else {
            g.press(action)
        }
        return true
    }

    // ---- Picker ----
    Item {
        anchors.fill: parent
        visible: !page.playing
        Text {
            id: heading
            text: "RETRO GAMES"; color: T.cyan; font.pixelSize: 36 * page.u; font.bold: true; font.letterSpacing: 2
        }
        Text {
            anchors { left: heading.right; leftMargin: 24 * page.u; baseline: heading.baseline }
            text: "Original arcade games, built in and playable with the remote"
            color: T.muted; font.pixelSize: 22 * page.u
        }
        Row {
            anchors.centerIn: parent; anchors.verticalCenterOffset: 24 * page.u
            spacing: 48 * page.u
            Repeater {
                model: page.games
                Item {
                    id: card
                    required property int index
                    required property var modelData
                    readonly property bool focused: index === page.selected
                    width: 500 * page.u; height: 620 * page.u
                    scale: focused ? 1.05 : 1.0
                    z: focused ? 1 : 0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                    Rectangle {
                        anchors.fill: parent; radius: 18 * page.u
                        color: card.focused ? "#12345e" : "#0a1d3a"; opacity: 0.95
                        border.width: card.focused ? 6 * page.u : 3 * page.u
                        border.color: card.focused ? T.yellow : card.modelData.color
                    }
                    Rectangle { // cabinet "screen"
                        id: screen
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 22 * page.u }
                        height: 240 * page.u; radius: 12 * page.u
                        gradient: Gradient { GradientStop { position: 0; color: "#0b2a55" } GradientStop { position: 1; color: "#030c1c" } }
                        border.color: card.modelData.color; border.width: 2 * page.u
                        Glyph { anchors.centerIn: parent; width: 170 * page.u; height: width; kind: card.modelData.glyph
                            color: card.focused ? T.yellow : card.modelData.color }
                    }
                    Column {
                        anchors { left: parent.left; right: parent.right; top: screen.bottom; margins: 26 * page.u; topMargin: 22 * page.u }
                        spacing: 10 * page.u
                        Text { text: card.modelData.title.toUpperCase(); color: card.focused ? T.yellow : T.text
                            font.pixelSize: 36 * page.u; font.bold: true; font.letterSpacing: 2 }
                        Text { text: card.modelData.tag; color: card.modelData.color; font.pixelSize: 22 * page.u; font.bold: true }
                        Text { width: parent.width; text: card.modelData.blurb; color: T.muted; font.pixelSize: 20 * page.u
                            wrapMode: Text.WordWrap; lineHeight: 1.1; height: 90 * page.u }
                        Text { text: card.modelData.controls; color: T.text; font.pixelSize: 20 * page.u }
                    }
                    Text {
                        anchors { left: parent.left; bottom: parent.bottom; margins: 26 * page.u }
                        text: { const b = page.bestFor(card.modelData.id); return b > 0 ? "BEST  " + b : "NO SCORE YET" }
                        color: T.yellow; font.pixelSize: 24 * page.u; font.bold: true; font.family: "monospace"
                    }
                }
            }
        }
    }

    // ---- Running game ----
    Rectangle {
        id: cabinet
        visible: !!page.playing
        anchors.fill: parent
        radius: 16 * page.u; color: "transparent"
        border.color: page.current ? page.current.color : T.cyan; border.width: 4 * page.u
        Loader {
            id: game
            anchors.fill: parent; anchors.margins: cabinet.border.width
            active: !!page.playing
            source: page.current ? page.current.file : ""
            onLoaded: { item.u = Qt.binding(() => page.u); item.paused = Qt.binding(() => page.paused); item.best = scores.best(page.playing) }
        }
        Connections {
            target: game.item
            ignoreUnknownSignals: true
            function onOverChanged() { if (game.item.over) page.finish(); else game.item.best = scores.best(page.playing) }
        }

        // Pause / game over card
        Rectangle {
            anchors.fill: parent; anchors.margins: cabinet.border.width
            visible: page.paused || (game.item && game.item.over)
            color: "#000000"; opacity: 0.6
        }
        Rectangle {
            visible: page.paused || (game.item && game.item.over)
            anchors.centerIn: parent
            width: 720 * page.u; height: overlayText.implicitHeight + 90 * page.u; radius: 18 * page.u
            color: "#0a1d3a"; border.color: T.yellow; border.width: 5 * page.u
            Column {
                id: overlayText
                anchors.centerIn: parent; spacing: 14 * page.u
                Text { anchors.horizontalCenter: parent.horizontalCenter
                    text: page.paused ? "PAUSED" : "GAME OVER"; color: page.paused ? T.cyan : T.pink
                    font.pixelSize: 64 * page.u; font.bold: true; font.letterSpacing: 6 }
                Text { anchors.horizontalCenter: parent.horizontalCenter
                    text: game.item ? "SCORE  " + game.item.score : ""; color: T.yellow; font.pixelSize: 36 * page.u; font.bold: true; font.family: "monospace" }
                Text { anchors.horizontalCenter: parent.horizontalCenter
                    visible: !page.paused; text: page.newBest ? "NEW BEST!" : "BEST  " + (page.playing ? page.bestFor(page.playing) : 0)
                    color: page.newBest ? T.ok : T.muted; font.pixelSize: 26 * page.u; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter
                    text: page.paused ? "OK resume   •   Back quit" : "OK play again   •   Back quit"
                    color: T.text; font.pixelSize: 24 * page.u }
            }
        }
    }
}
