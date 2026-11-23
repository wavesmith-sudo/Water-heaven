import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

TextField {
    id: f
    property real u: 1
    font.pixelSize: 26 * u
    color: T.text
    placeholderTextColor: "#6f8fb5"
    selectionColor: T.magenta
    leftPadding: 18 * u; rightPadding: 18 * u; topPadding: 12 * u; bottomPadding: 12 * u
    background: Rectangle {
        radius: 10 * f.u
        color: f.activeFocus ? "#12345e" : "#0a1d3a"
        border.width: f.activeFocus ? 4 : 2
        border.color: f.activeFocus ? T.yellow : T.cyan
    }
}
