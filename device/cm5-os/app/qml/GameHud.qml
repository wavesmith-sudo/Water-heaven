import QtQuick
import "Theme.js" as T

// Label/value pair used in the game heads-up displays.
Column {
    property string label
    property string value
    property color color: T.text
    property real u: 1
    spacing: 2 * u
    Text { text: parent.label; color: T.muted; font.pixelSize: 18 * parent.u; font.bold: true; font.letterSpacing: 2 }
    Text { text: parent.value; color: parent.color; font.pixelSize: 40 * parent.u; font.bold: true; font.family: "monospace" }
}
