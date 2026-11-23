import QtQuick
import "Theme.js" as T

Item {
    id: card
    property var item: ({})
    property bool focused: false
    scale: focused ? 1.06 : 1.0
    z: focused ? 1 : 0
    Behavior on scale { NumberAnimation { duration: 110 } }

    Rectangle {
        id: frame
        anchors.fill: parent; anchors.margins: 8; radius: 10
        color: card.focused ? "#15355f" : "#0c2244"
        border.width: card.focused ? 4 : 1
        border.color: card.focused ? T.yellow : "#2f5580"
        clip: true

        Rectangle { id: art; anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 } height: parent.height - 64; radius: 6; color: "#06162e"
            Text { anchors.centerIn: parent; visible: poster.status !== Image.Ready; text: (card.item.name || "?").charAt(0); color: T.magenta; font.pixelSize: 56; font.bold: true }
            Image { id: poster; anchors.fill: parent; source: card.item.image || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 400; clip: true }
            Rectangle { // resume progress
                visible: (card.item.playedPct || 0) > 0
                anchors { left: parent.left; bottom: parent.bottom; right: parent.right } height: 5; color: "#55000000"
                Rectangle { height: parent.height; color: T.pink; width: parent.width * Math.min(100, card.item.playedPct || 0) / 100 }
            }
        }
        Column {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
            Text { width: parent.width; text: card.item.name || ""; elide: Text.ElideRight; color: T.text; font.pixelSize: 17; font.bold: card.focused }
            Text { width: parent.width; text: card.item.subtitle || ""; elide: Text.ElideRight; color: T.muted; font.pixelSize: 14; visible: text.length > 0 }
        }
    }
}
