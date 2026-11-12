import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// An icon button from the board (AppNotes.dc): 30 by 30 on the formatting
// bar, 34 by 34 beside the list heading and on the right of the bar, radius
// 8. `lit` draws the icon in the accent, as the pin of a pinned note is.
T.Button {
    id: control

    property string iconName
    property real iconSize: 16
    property real side: 34
    property bool lit: false
    property bool active: false
    property string tip

    implicitWidth: side
    implicitHeight: side
    padding: 0
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    Accessible.name: tip
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: UI.Style.radiusSm
        color: control.down ? UI.Style.surfacePressed : control.active ? UI.Style.surfaceSelected : control.hovered && control.enabled ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        NotesIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.lit ? UI.Style.accent : UI.Style.iconInk
        }
    }

    UI.Tooltip {
        text: control.tip
        visible: control.hovered && control.tip.length > 0 && !control.down
    }
}
