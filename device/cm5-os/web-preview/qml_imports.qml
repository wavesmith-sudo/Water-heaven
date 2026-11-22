// Not loaded at runtime. Qt for WebAssembly links QML plugins statically, and Qt's import
// scanner only looks under this folder, not ../app/qml. Listing the device QML's imports here
// makes the build link them (without this the page fails with "plugin qtquick2plugin not found").
// Keep in sync with the imports used in ../app/qml.
import QtQuick
import QtMultimedia

Item {}
