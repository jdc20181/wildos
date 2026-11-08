import QtQuick
import WildOS.UI as UI

// A button in the clock flyout. The notifications card lives in a window that
// never takes keyboard focus (so clicking it cannot close the flyout), so
// keyboard focus is tracked by the flyout itself: "current" marks the button
// the keyboard is on, and the flyout calls activate() for Enter and Space.
Item {
    id: button

    property real radius: 8
    property real fillWidth: width
    property real fillHeight: height
    property color baseColor: "transparent"
    property color hoverColor: UI.Style.surfaceHover
    property color pressedColor: UI.Style.surfacePressed
    readonly property bool current: Nav.current === button
    property bool clickable: true
    property alias hovered: mouse.containsMouse
    property alias pressed: mouse.pressed
    property string accessibleName
    readonly property bool showFocus: current && Nav.keyboard

    signal clicked()

    function activate(): void {
        if (clickable && enabled && visible) {
            clicked();
        }
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: true
    Accessible.focused: current
    Accessible.onPressAction: activate()

    Rectangle {
        anchors.centerIn: parent
        width: button.fillWidth
        height: button.fillHeight
        radius: button.radius
        color: button.pressed ? button.pressedColor
             : button.hovered && button.clickable ? button.hoverColor
             : button.baseColor
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: button.showFocus
            targetRadius: button.radius
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.activate()
    }
}
