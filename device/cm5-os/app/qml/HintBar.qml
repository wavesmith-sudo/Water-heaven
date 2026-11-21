import QtQuick
import "Theme.js" as T

Row {
    id: bar
    property var hints: []   // [{key: "A", label: "Select", color: "#3fdc7c"}]
    spacing: 34
    Repeater {
        model: bar.hints
        Row {
            required property var modelData
            spacing: 10
            Rectangle {
                width: Math.max(34, keyText.implicitWidth + 16); height: 34; radius: 17
                color: modelData.color || "transparent"; border.color: modelData.color ? "transparent" : T.muted; border.width: 2
                Text { id: keyText; anchors.centerIn: parent; text: modelData.key; color: "white"; font.pixelSize: 17; font.bold: true }
            }
            Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: T.text; font.pixelSize: 20 }
        }
    }
}
