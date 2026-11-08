import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The 44px round buttons at the bottom right of the Ctrl+Alt+Del screen
// (network, accessibility, power), white 0.10 discs with an 18px icon.
// Each opens a Wild OS menu above it; its name shows as a tooltip.
T.AbstractButton {
    id: control

    property string iconName
    property string label
    property var entries: []

    implicitWidth: 44
    implicitHeight: 44
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.role: Accessible.ButtonMenu

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    onClicked: {
        if (menu.opened) {
            menu.close();
            return;
        }
        menu.entries = control.entries;
        menu.popup(control, control.width - menu.implicitWidth, -menu.implicitHeight - 8);
    }

    background: Rectangle {
        radius: 22
        color: control.down ? UI.Style.controlOnWallpaperPressed
             : control.hovered || menu.opened ? UI.Style.controlOnWallpaperSelected
             : Qt.rgba(UI.Style.inkOnWallpaper.r, UI.Style.inkOnWallpaper.g, UI.Style.inkOnWallpaper.b, 0.10)
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: 22
        }
    }

    contentItem: Item {
        UI.Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: 18
            strokeWidth: 2
            color: UI.Style.inkOnWallpaper
        }
    }

    UI.Tooltip {
        visible: control.hovered && !menu.opened
        text: control.label
        y: -implicitHeight - 8
    }

    UI.Menu {
        id: menu
        minimumWidth: 220
    }
}
