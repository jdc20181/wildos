import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// One of the security options on the Ctrl+Alt+Del screen (CtrlAltDel.dc):
// 300 by 48, 17px white words at 18 from the left, and the 0.14 white
// highlight the board draws on Task Manager for the one under the pointer
// or the keyboard.
T.AbstractButton {
    id: control

    property bool previewHover: false

    implicitWidth: 300
    implicitHeight: 48
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    Accessible.role: Accessible.Button

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    // The pointer takes the keyboard's place, so only one option is ever lit
    // and Enter picks the one that looks picked.
    onHoveredChanged: if (hovered) forceActiveFocus()

    background: Rectangle {
        radius: 10
        color: control.down ? UI.Style.controlOnWallpaperPressed
             : control.hovered || control.activeFocus || control.previewHover ? UI.Style.controlOnWallpaperSelected
             : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: 10
        }
    }

    contentItem: UI.Label {
        leftPadding: 18
        rightPadding: 18
        text: control.text
        size: 17
        color: UI.Style.inkOnWallpaper
    }
}
