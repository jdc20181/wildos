import QtQuick
import QtQuick.VirtualKeyboard

// Loaded on demand, so a system without the Qt virtual keyboard module just
// hides the accessibility button instead of failing to start the greeter.
InputPanel {
    id: panel

    property bool wanted: false

    active: wanted && Qt.inputMethod.visible
    visible: active
    width: parent ? parent.width : 0
    y: active ? parent.height - height : parent.height

    Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
}
