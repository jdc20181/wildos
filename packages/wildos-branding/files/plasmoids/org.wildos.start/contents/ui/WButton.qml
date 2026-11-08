import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// Start's plain area button: the account pill, the section pills and the
// command rows beside a search result, which hold their own content rather
// than one label. Hover and press come from WildOS.UI; the focus ring shows
// for keyboard focus only, as everywhere in Wild OS.
T.AbstractButton {
    id: button

    property real radius: Style.radiusItem
    property color baseColor: "transparent"
    property bool highlighted: false
    property string accessibleName

    signal rightClicked(real x, real y)

    padding: 0
    hoverEnabled: true
    // Start moves the keyboard between its parts itself (StartMenu
    // tabStops), so Qt's own Tab chain must not take the key first.
    focusPolicy: Qt.NoFocus
    Accessible.role: Accessible.Button
    Accessible.name: accessibleName

    // Templates buttons take Space; Start's rows take Enter too, as before.
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    background: Rectangle {
        radius: button.radius
        color: button.down || button.highlighted ? UI.Style.surfacePressed
             : button.hovered ? UI.Style.surfaceHover
             : button.baseColor
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: button.visualFocus || (button.activeFocus && Nav.keyboard)
            targetRadius: button.radius
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point, buttonPressed) => button.rightClicked(point.position.x, point.position.y)
    }
}
