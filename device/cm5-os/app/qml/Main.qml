import QtQuick
import "Theme.js" as T

Window {
    id: root
    visible: true
    visibility: Window.FullScreen
    width: 1920; height: 1080
    title: "Retro Arcade Media OS"
    color: T.bg

    // Layout is designed at 1080p and scaled, so 720p/4K panels keep proportions.
    readonly property real u: Math.min(width / 1920, height / 1080)

    property string page: "Home"          // Home | Library | Episodes | Player | Settings | Games
    property int selected: 0
    property string currentCategory: ""
    property var currentSeries: null      // {id, name} while browsing episodes
    property int seriesReturnIndex: 0
    property string playerReturnPage: "Library"

    readonly property var categories: [
        {label: "Movies", icon: "movies"},
        {label: "TV Shows", icon: "tv"},
        {label: "Music", icon: "music"},
        {label: "Retro Games", icon: "games"},
        {label: "Continue Watching", icon: "resume"},
        {label: "Settings", icon: "settings"}
    ]

    function openPage(p) { page = p; selected = 0; keys.forceActiveFocus() }

    function openCategory(name) {
        if (name === "Retro Games") { openPage("Games"); return }
        if (name === "Settings") { openPage("Settings"); serverField.forceActiveFocus(); return }
        currentCategory = name
        openPage("Library")
        if (name === "Continue Watching") jellyfin.continueWatching()
        else jellyfin.browse(name)
    }

    function activate(item) {
        if (!item) return
        if (item.type === "Series") {
            currentSeries = {id: item.id, name: item.name}
            seriesReturnIndex = selected
            openPage("Episodes")
            jellyfin.browseEpisodes(item.id)
        } else if (["Movie", "Episode", "Video", "Audio", "MusicVideo"].indexOf(item.type) >= 0) {
            play(item)
        }
    }

    function play(item) {
        const url = jellyfin.playbackUrl(item.id, item.type)
        if (!url) return
        playerReturnPage = page
        playerLoader.playItem = item
        playerLoader.playSource = url
        playerLoader.active = true
        page = "Player"
        keys.forceActiveFocus()
    }

    function stopPlayback() {
        if (playerLoader.status === Loader.Ready) playerLoader.item.stop()
        playerLoader.active = false
    }

    function back() {
        if (page === "Player") { stopPlayback(); page = playerReturnPage; keys.forceActiveFocus() }
        else if (page === "Episodes") { page = "Library"; jellyfin.browse(currentCategory); selected = seriesReturnIndex; keys.forceActiveFocus() }
        else if (page !== "Home") { const from = page; openPage("Home"); selected = Math.max(0, categories.findIndex(c => c.label === (from === "Library" ? currentCategory : from === "Games" ? "Retro Games" : "Settings"))) }
    }

    Connections {
        target: jellyfin
        function onLoggedIn() { root.openPage("Home") }
    }

    Backdrop { anchors.fill: parent; dim: root.page !== "Home"; visible: root.page !== "Player" }

    // ---- Header -----------------------------------------------------------------
    Item {
        id: header
        visible: root.page !== "Player"
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 48 * root.u }
        height: 64 * root.u
        Row {
            spacing: 16 * root.u; anchors.verticalCenter: parent.verticalCenter
            Glyph { width: 52 * root.u; height: width; kind: "games"; color: T.magenta; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "RETRO ARCADE"; color: T.text; font.pixelSize: 34 * root.u; font.bold: true; font.letterSpacing: 2; anchors.verticalCenter: parent.verticalCenter }
            Text { visible: root.page !== "Home"; text: "/  " + (root.page === "Library" ? root.currentCategory : root.page === "Episodes" && root.currentSeries ? root.currentSeries.name : root.page); color: T.muted; font.pixelSize: 28 * root.u; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            spacing: 22 * root.u
            Text { text: jellyfin.signedIn ? "● " + (jellyfin.userName || "Signed in") : "○ Not signed in"; color: jellyfin.signedIn ? T.ok : T.muted; font.pixelSize: 22 * root.u; anchors.verticalCenter: parent.verticalCenter }
            Text { id: clock; color: T.text; font.pixelSize: 30 * root.u; font.bold: true; anchors.verticalCenter: parent.verticalCenter
                text: Qt.formatTime(new Date(), "h:mm AP")
                Timer { interval: 15000; running: true; repeat: true; onTriggered: clock.text = Qt.formatTime(new Date(), "h:mm AP") }
            }
        }
    }

    // ---- Home -------------------------------------------------------------------
    Item {
        anchors.fill: parent
        visible: root.page === "Home"
        Text {
            anchors.horizontalCenter: parent.horizontalCenter; y: parent.height * 0.17
            text: "STREAM  •  PLAY  •  REPEAT"; color: T.cyan; font.pixelSize: 40 * root.u; font.bold: true; font.letterSpacing: 4
        }
        Row {
            id: tiles
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.56
            spacing: 26 * root.u
            Repeater {
                model: root.categories
                CategoryTile {
                    required property int index
                    required property var modelData
                    width: 210 * root.u; height: 220 * root.u
                    label: modelData.label; icon: modelData.icon
                    focused: root.page === "Home" && index === root.selected
                }
            }
        }
    }

    // ---- Library / Episodes ---------------------------------------------------
    Item {
        id: libraryPage
        visible: root.page === "Library" || root.page === "Episodes"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: hints.top; leftMargin: 48 * root.u; rightMargin: 48 * root.u; topMargin: 24 * root.u; bottomMargin: 16 * root.u }

        Text {
            id: libraryTitle
            text: (root.page === "Episodes" && root.currentSeries ? root.currentSeries.name : root.currentCategory).toUpperCase()
            color: T.cyan; font.pixelSize: 36 * root.u; font.bold: true; font.letterSpacing: 2
        }
        GridView {
            id: library
            anchors { left: parent.left; right: parent.right; top: libraryTitle.bottom; bottom: details.top; topMargin: 16 * root.u }
            readonly property bool wide: root.page === "Episodes"
            cellWidth: wide ? 380 * root.u : 250 * root.u
            cellHeight: wide ? 300 * root.u : 400 * root.u
            clip: true
            interactive: false
            model: jellyfin.items
            currentIndex: root.selected
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
            highlightMoveDuration: 0
            delegate: PosterCard {
                required property int index
                required property var modelData
                width: library.cellWidth; height: library.cellHeight
                item: modelData
                focused: index === root.selected
            }
        }
        Text {
            anchors.centerIn: library
            visible: !jellyfin.busy && jellyfin.items.length === 0
            horizontalAlignment: Text.AlignHCenter
            text: jellyfin.error.length > 0 ? jellyfin.error
                  : !jellyfin.signedIn ? "Sign in to Jellyfin in Settings to browse your library"
                  : "Nothing here yet"
            color: jellyfin.error.length > 0 ? T.danger : T.muted; font.pixelSize: 28 * root.u
        }
        Text {
            id: details
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 70 * root.u
            readonly property var current: jellyfin.items.length > root.selected ? jellyfin.items[root.selected] : null
            text: current ? current.overview : ""
            color: T.muted; font.pixelSize: 20 * root.u; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
    }

    // ---- Settings ---------------------------------------------------------------
    FocusScope {
        id: settingsPage
        visible: root.page === "Settings"
        anchors { left: parent.left; top: header.bottom; leftMargin: 120 * root.u; topMargin: 40 * root.u }
        width: 900 * root.u; height: 700 * root.u
        // Escape/Back leave the page; Backspace stays with the text field for editing.
        Keys.onPressed: function(e) {
            if (e.key === Qt.Key_Escape || e.key === Qt.Key_Back) { root.back(); e.accepted = true }
        }
        Column {
            spacing: 18 * root.u
            Text { text: "JELLYFIN CONNECTION"; color: T.cyan; font.pixelSize: 34 * root.u; font.bold: true }
            Text { text: "Device ID " + jellyfin.deviceId; color: T.muted; font.pixelSize: 18 * root.u }
            ArcadeField { id: serverField; u: root.u; width: 760 * root.u; text: jellyfin.server; placeholderText: "https://media.example.com"
                KeyNavigation.down: userField; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase }
            ArcadeField { id: userField; u: root.u; width: 760 * root.u; placeholderText: "Username"
                KeyNavigation.up: serverField; KeyNavigation.down: passwordField; inputMethodHints: Qt.ImhNoAutoUppercase }
            ArcadeField { id: passwordField; u: root.u; width: 760 * root.u; placeholderText: "Password"; echoMode: TextInput.Password
                KeyNavigation.up: userField; KeyNavigation.down: connectButton
                onAccepted: connectButton.clicked() }
            Row {
                spacing: 20 * root.u
                ArcadeButton { id: connectButton; u: root.u; text: jellyfin.busy ? "Connecting…" : "Connect"; enabled: !jellyfin.busy
                    KeyNavigation.up: passwordField; KeyNavigation.right: signOutButton
                    onClicked: { jellyfin.server = serverField.text; if (jellyfin.error.length === 0) jellyfin.login(userField.text, passwordField.text); passwordField.text = "" } }
                ArcadeButton { id: signOutButton; u: root.u; text: "Sign out"; visible: jellyfin.signedIn
                    KeyNavigation.left: connectButton; KeyNavigation.up: passwordField
                    onClicked: jellyfin.logout() }
            }
            Text { text: jellyfin.error; color: T.danger; font.pixelSize: 22 * root.u; visible: text.length > 0 }
            Text { text: jellyfin.signedIn ? "Signed in as " + jellyfin.userName : "Not signed in"; color: jellyfin.signedIn ? T.ok : T.muted; font.pixelSize: 22 * root.u }
            Text { text: "Use HTTPS outside a trusted local network."; color: T.muted; font.pixelSize: 20 * root.u }
        }
    }

    // ---- Games ------------------------------------------------------------------
    Column {
        visible: root.page === "Games"
        anchors.centerIn: parent
        spacing: 16 * root.u
        Glyph { width: 140 * root.u; height: width; kind: "games"; color: T.yellow; anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: "RETRO GAMES"; color: T.yellow; font.pixelSize: 44 * root.u; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: "RetroArch integration is a future milestone."; color: T.text; font.pixelSize: 26 * root.u; anchors.horizontalCenter: parent.horizontalCenter }
    }

    // ---- Player -----------------------------------------------------------------
    Loader {
        id: playerLoader
        anchors.fill: parent
        active: false
        property var playItem: null
        property url playSource
        sourceComponent: PlayerView {
            item: playerLoader.playItem
            source: playerLoader.playSource
            u: root.u
            onDone: root.back()
        }
    }

    // ---- Footer hints + loading -------------------------------------------------
    HintBar {
        id: hints
        visible: root.page !== "Player"
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 36 * root.u }
        scale: root.u; transformOrigin: Item.Bottom
        hints: root.page === "Settings"
               ? [{key: "▲ ▼", label: "Move"}, {key: "Esc", label: "Back", color: "#e0303c"}]
               : [{key: "◀ ▶", label: "Navigate"}, {key: "A", label: "Select", color: T.ok}, {key: "B", label: "Back", color: "#e0303c"}]
    }
    Text { visible: jellyfin.busy; anchors { right: parent.right; bottom: parent.bottom; margins: 40 * root.u } text: "LOADING…"; color: T.cyan; font.pixelSize: 24 * root.u; font.bold: true }

    // ---- Remote / keyboard navigation ------------------------------------------
    // D-pad = arrows, OK = Return/Enter/Select, Back = Escape/Backspace/Back.
    // Media keys are handled where the player is active. HID mapping for the custom
    // Bluetooth remote is still TODO (docs/MEDIA_CENTER.md).
    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(e) {
            if (root.page === "Settings") return
            if (e.key === Qt.Key_Escape || e.key === Qt.Key_Backspace || e.key === Qt.Key_Back) { root.back(); e.accepted = true; return }
            if (root.page === "Player") {
                const pv = playerLoader.status === Loader.Ready ? playerLoader.item : null
                if (!pv) return
                if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter || e.key === Qt.Key_Space || e.key === Qt.Key_Select || e.key === Qt.Key_MediaTogglePlayPause || e.key === Qt.Key_MediaPlay || e.key === Qt.Key_MediaPause)
                    pv.togglePause()
                else if (e.key === Qt.Key_Right || e.key === Qt.Key_MediaNext) pv.seekBy(10000)
                else if (e.key === Qt.Key_Left || e.key === Qt.Key_MediaPrevious) pv.seekBy(-10000)
                else if (e.key === Qt.Key_MediaStop) root.back()
                else return
                e.accepted = true
                return
            }
            const count = root.page === "Home" ? root.categories.length : (libraryPage.visible ? jellyfin.items.length : 0)
            const columns = root.page === "Home" ? count : Math.max(1, Math.floor(library.width / library.cellWidth))
            if (count === 0 && e.key !== Qt.Key_Home) return
            if (e.key === Qt.Key_Right) root.selected = Math.min(count - 1, root.selected + 1)
            else if (e.key === Qt.Key_Left) root.selected = Math.max(0, root.selected - 1)
            else if (e.key === Qt.Key_Down) root.selected = Math.min(count - 1, root.selected + columns)
            else if (e.key === Qt.Key_Up) root.selected = Math.max(0, root.selected - columns)
            else if (e.key === Qt.Key_Home) root.openPage("Home")
            else if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter || e.key === Qt.Key_Select) {
                if (root.page === "Home") root.openCategory(root.categories[root.selected].label)
                else if (libraryPage.visible) root.activate(jellyfin.items[root.selected])
            } else return
            e.accepted = true
        }
    }
}
