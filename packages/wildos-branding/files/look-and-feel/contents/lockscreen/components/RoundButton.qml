import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The 44px round glass button in the bottom right corner (Lock.dc). Its
// name shows as a tooltip above it, since the screen's foot leaves no room
// below.
T.AbstractButton {
    id: control

    property Item backdrop: null
    property string iconName
    property string label

    implicitWidth: 44
    implicitHeight: 44
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.role: Accessible.Button

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    background: Item {
        Glass {
            anchors.fill: parent
            backdrop: control.backdrop
            tint: control.down ? UI.Style.glassOnWallpaperPressed
                 : control.hovered || control.checked ? UI.Style.glassOnWallpaperHover
                 : UI.Style.glassOnWallpaper
            blurRadius: 64
            Behavior on tint { ColorAnimation { duration: UI.Style.hoverDuration } }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: control.height / 2
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
        visible: control.hovered && control.label.length > 0 && !control.checked
        text: control.label
        y: -implicitHeight - 8
    }
}
