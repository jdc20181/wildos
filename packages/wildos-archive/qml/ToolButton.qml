import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A button from the board's toolbar and pane (AppArchive.dc): 32px tall,
// radius 6, a 16px line icon and 13px text 7px after it; `accent` fills it
// with the accent for the main action, 16px in, a 15px icon and semibold
// text. Without text it is a square icon button.
T.Button {
    id: control

    property string iconName
    property bool accent: false
    property real side: 32
    property real iconSize: accent ? 15 : 16
    property string tip

    implicitHeight: side
    implicitWidth: text.length > 0 ? row.implicitWidth + leftPadding + rightPadding : side
    leftPadding: text.length > 0 ? (accent ? 16 : 10) : 0
    rightPadding: leftPadding
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    Accessible.name: text.length > 0 ? text : tip
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: control.text.length > 0 ? 6 : UI.Style.radiusSm
        color: control.accent
            ? (control.down ? UI.Style.accentPressed : control.hovered && control.enabled ? UI.Style.accentHover : UI.Style.accent)
            : control.down ? ArchiveStyle.rowPressed : control.hovered && control.enabled ? ArchiveStyle.rowHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.accent ? 8 : 7

            ArchiveIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                name: control.iconName
                size: control.iconSize
                color: control.accent ? UI.Style.textOnAccent : ArchiveStyle.iconInk
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.text.length > 0
                text: control.text
                size: 13
                font.weight: control.accent ? UI.Style.weightSemiBold : UI.Style.weightRegular
                color: control.accent ? UI.Style.textOnAccent : UI.Style.ink
            }
        }
    }

    UI.Tooltip {
        text: control.tip
        visible: control.hovered && control.tip.length > 0 && !control.down
    }
}
