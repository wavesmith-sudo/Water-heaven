import QtQuick
import "Theme.js" as T

Item {
    id: tile
    property string label
    property string icon
    property bool focused: false
    width: 180; height: 190
    scale: focused ? 1.1 : 1.0
    z: focused ? 1 : 0
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

    Rectangle {
        anchors.fill: parent; radius: 14
        color: tile.focused ? "#2a1a4a" : "#160d2e"; opacity: 0.92
        border.width: tile.focused ? 5 : 3
        border.color: tile.focused ? T.yellow : T.cyan
    }
    Glyph {
        id: glyph
        width: tile.width * 0.45; height: width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: tile.height * 0.13
        kind: tile.icon
        color: tile.focused ? T.yellow : T.magenta
    }
    Text {
        anchors.top: glyph.bottom; anchors.topMargin: tile.height * 0.06
        anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 6
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; maximumLineCount: 2
        text: tile.label.toUpperCase()
        color: tile.focused ? T.yellow : T.text
        font.pixelSize: Math.max(14, tile.height * 0.095); font.bold: true; font.letterSpacing: 1
        fontSizeMode: Text.HorizontalFit; minimumPixelSize: 10
    }
}
