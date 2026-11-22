import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

Button {
    id: b
    property real u: 1
    font.pixelSize: 24 * u; font.bold: true
    leftPadding: 30 * u; rightPadding: 30 * u; topPadding: 12 * u; bottomPadding: 12 * u
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    contentItem: Text {
        text: b.text.toUpperCase(); font: b.font
        color: !b.enabled ? T.muted : b.activeFocus ? T.yellow : T.text
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 10 * b.u
        color: b.activeFocus ? "#2a1a4a" : "#160d2e"
        border.width: b.activeFocus ? 4 : 2
        border.color: b.activeFocus ? T.yellow : T.magenta
    }
}
