import QtQuick
import QtMultimedia
import "Theme.js" as T

// Created on demand by a Loader in Main.qml, so the home UI still starts when the
// multimedia backend is missing or broken (on Ubuntu Qt 6.4, constructing MediaPlayer
// without GStreamer base plugins segfaults).
Item {
    id: view
    property var item: null          // Jellyfin item map
    property real u: 1
    signal done()                    // playback ended or user backed out

    // Set from jellyfin.playbackReady: DirectPlay | DirectStream | Transcode.
    property string method: ""
    property string transcodeReasons: ""
    property bool fellBack: false    // already retried once with a server transcode
    property bool started: false     // reportPlaybackStart sent
    property bool stopped: false     // user backed out; late player errors are ignored
    property int resumeTarget: item ? (item.resumeMs || 0) : 0
    property bool resumeApplied: false
    property int lastPosition: 0
    readonly property bool isVideo: item !== null && item.type !== "Audio"

    function togglePause() { player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play() }
    function seekBy(ms) { player.setPosition(Math.max(0, Math.min(player.duration, player.position + ms))) }
    function stop() {
        stopped = true
        if (item) jellyfin.reportPlaybackStopped(item.id, Math.max(player.position, lastPosition))
        player.stop()
    }
    function fmt(ms) {
        const s = Math.floor(ms / 1000), h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, ss = s % 60
        return (h > 0 ? h + ":" + (m < 10 ? "0" : "") : "") + m + ":" + (ss < 10 ? "0" : "") + ss
    }
    // Direct play failed on the device (no demuxer/decoder, corrupt stream): ask the server
    // for an H.264/AAC HLS transcode once, and continue from where playback got to.
    function fallBack(why) {
        if (stopped || fellBack || method === "Transcode" || !item) return false
        console.info("cm5-home: " + method + " failed (" + why + "), retrying with a server transcode")
        fellBack = true
        resumeTarget = Math.max(lastPosition, resumeTarget)
        resumeApplied = false
        playerError.text = ""
        player.stop()
        player.source = ""
        jellyfin.requestPlayback(item.id, item.type, false)
        return true
    }

    Component.onCompleted: if (item) jellyfin.requestPlayback(item.id, item.type, true)

    Connections {
        target: jellyfin
        function onPlaybackReady(info) {
            if (view.stopped || !view.item || info.itemId !== view.item.id) return
            view.method = info.method
            view.transcodeReasons = info.transcodeReasons
            player.source = info.url
            player.play()
            if (!view.started) {
                view.started = true
                jellyfin.reportPlaybackStart(view.item.id, view.resumeTarget)
            }
        }
        function onPlaybackFailed(itemId, message) {
            if (view.item && itemId === view.item.id) playerError.text = message
        }
    }

    Rectangle { anchors.fill: parent; color: "black" }
    VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
    MediaPlayer {
        id: player
        videoOutput: video
        audioOutput: AudioOutput {}
        onPositionChanged: {
            if (player.position > 0) view.lastPosition = player.position
            // A container GStreamer can open but whose video it cannot decode plays as audio only.
            // hasVideo is not reliable at LoadedMedia, so check once playback has run a while.
            if (view.isVideo && !player.hasVideo && player.position > 3000) view.fallBack("no decodable video stream")
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                if (!view.resumeApplied && view.item) {
                    view.resumeApplied = true
                    if (view.resumeTarget > 0 && seekable) setPosition(view.resumeTarget)
                }
            }
            if (mediaStatus === MediaPlayer.InvalidMedia && view.fallBack("invalid media")) return
            if (mediaStatus === MediaPlayer.EndOfMedia) view.done()
        }
        onErrorOccurred: function(error, errorString) {
            if (view.stopped || view.fallBack(errorString)) return
            playerError.text = "Playback failed: " + errorString
        }
    }
    Timer {
        interval: 10000; repeat: true; running: view.item !== null && view.started
        onTriggered: jellyfin.reportPlaybackProgress(view.item.id, player.position, player.playbackState !== MediaPlayer.PlayingState)
    }
    Text {
        anchors.centerIn: parent; visible: player.source.toString().length === 0 && playerError.text.length === 0
        text: view.fellBack ? "SWITCHING TO SERVER TRANSCODE…" : "STARTING…"; color: T.cyan; font.pixelSize: 28 * view.u; font.bold: true
    }
    Item {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 48 * view.u }
        height: 120 * view.u
        Text { id: playerError; anchors.bottom: progress.top; anchors.bottomMargin: 12 * view.u; color: T.danger; font.pixelSize: 24 * view.u }
        Text { anchors.bottom: progress.top; anchors.bottomMargin: 12 * view.u; anchors.left: parent.left; visible: playerError.text.length === 0 && view.method.length > 0
            text: view.method === "Transcode" ? "SERVER TRANSCODE" : view.method === "DirectStream" ? "DIRECT STREAM" : "DIRECT PLAY"
            color: view.method === "Transcode" ? T.yellow : T.ok; font.pixelSize: 20 * view.u; font.bold: true }
        Text { anchors.bottom: progress.top; anchors.bottomMargin: 12 * view.u; anchors.right: parent.right; visible: playerError.text.length === 0
            text: (view.item ? view.item.name + "   " : "") + view.fmt(player.position) + " / " + view.fmt(player.duration); color: T.text; font.pixelSize: 24 * view.u }
        Rectangle {
            id: progress
            anchors { left: parent.left; right: parent.right; bottom: playerHints.top; bottomMargin: 14 * view.u }
            height: 8 * view.u; radius: height / 2; color: "#40ffffff"
            Rectangle { height: parent.height; radius: parent.radius; color: T.pink; width: player.duration > 0 ? parent.width * player.position / player.duration : 0 }
        }
        HintBar { id: playerHints; anchors.bottom: parent.bottom; scale: view.u; transformOrigin: Item.BottomLeft
            hints: [{key: "OK", label: player.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play", color: T.ok}, {key: "◀ ▶", label: "Seek 10s"}, {key: "B", label: "Back", color: "#e0303c"}] }
    }
}
