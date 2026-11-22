// Not loaded at runtime. Qt for WebAssembly links QML plugins statically, and Qt's import
// scanner only looks under this folder, not ../app/qml. Listing the device QML's imports here
// makes the build link them (without this the page fails with "plugin qtquick2plugin not found").
// check_api.py fails the build if ../app/qml imports a module that is not listed here.
import QtQuick
import QtQuick.Controls.Basic
import QtMultimedia

Item {}
