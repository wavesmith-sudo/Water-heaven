import QtQuick
import QtMultimedia
import "Theme.js" as T

// Created on demand by a Loader in Main.qml, so the home UI still starts when the
// multimedia backend is missing or broken (on Ubuntu Qt 6.4, constructing MediaPlayer
// without GStreamer base plugins segfaults).
Item {
    id: view
    property var item: null          // Jellyfin item map
    property url source
    property real u: 1
    signal done()                    // playback ended or user backed out

    function togglePause() { player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play() }
    function seekBy(ms) { player.setPosition(Math.max(0, Math.min(player.duration, player.position + ms))) }
    function stop() {
        if (item) jellyfin.reportPlaybackStopped(item.id, player.position)
        player.stop()
    }
    function fmt(ms) {
        const s = Math.floor(ms / 1000), h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, ss = s % 60
        return (h > 0 ? h + ":" + (m < 10 ? "0" : "") : "") + m + ":" + (ss < 10 ? "0" : "") + ss
    }

    property bool resumeApplied: false
    Component.onCompleted: {
        player.source = view.source
        player.play()
        if (item) jellyfin.reportPlaybackStart(item.id, item.resumeMs || 0)
    }

    Rectangle { anchors.fill: parent; color: "black" }
    VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
    MediaPlayer {
        id: player
        videoOutput: video
        audioOutput: AudioOutput {}
        onMediaStatusChanged: {
            if (!view.resumeApplied && view.item && (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)) {
                view.resumeApplied = true
                if ((view.item.resumeMs || 0) > 0 && seekable) setPosition(view.item.resumeMs)
            }
            if (mediaStatus === MediaPlayer.EndOfMedia) view.done()
        }
        onErrorOccurred: function(error, errorString) { playerError.text = "Playback failed: " + errorString }
    }
    Timer {
        interval: 10000; repeat: true; running: view.item !== null
        onTriggered: jellyfin.reportPlaybackProgress(view.item.id, player.position, player.playbackState !== MediaPlayer.PlayingState)
    }
    Item {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 48 * view.u }
        height: 120 * view.u
        Text { id: playerError; anchors.bottom: progress.top; anchors.bottomMargin: 12 * view.u; color: T.danger; font.pixelSize: 24 * view.u }
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
