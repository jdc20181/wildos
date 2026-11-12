import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// An icon button from the board (AppMusic.dc): 34 by 34, radius 8, a 16px
// icon (18 for previous, next, the heart and More beside the album). `lit`
// draws the icon in the accent, as shuffle and repeat are when they are on.
T.Button {
    id: control

    property string iconName
    property real iconSize: 16
    property real side: 34
    property bool lit: false
    property bool filled: false
    property color iconColor: lit ? UI.Style.accent : UI.Style.iconInk
    property string tip

    implicitWidth: side
    implicitHeight: side
    padding: 0
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    Accessible.name: tip
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: UI.Style.radiusSm
        color: control.down ? UI.Style.surfacePressed : control.hovered && control.enabled ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        MusicIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            filled: control.filled
            color: control.iconColor
        }
    }

    UI.Tooltip {
        text: control.tip
        visible: control.hovered && control.tip.length > 0 && !control.down
    }
}
