import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// Play and Shuffle under an album's title (AppMusic.dc): 32px high, radius
// 6, 16px each side, a 15px icon and 13px text. Play is the accent fill;
// Shuffle is the quiet outlined one.
T.Button {
    id: control

    property string iconName
    property bool primary: false

    implicitHeight: 32
    implicitWidth: Math.max(64, implicitContentWidth + leftPadding + rightPadding)
    leftPadding: 16
    rightPadding: 16
    topPadding: 0
    bottomPadding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5
    Accessible.name: text
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: UI.Style.radiusXs
        color: control.primary
            ? (control.down ? UI.Style.accentPressed : control.hovered ? UI.Style.accentHover : UI.Style.accent)
            : control.down ? UI.Style.surfacePressed : control.hovered ? UI.Style.surfaceHover : UI.Style.surfaceControl
        border.width: control.primary ? 0 : 1
        border.color: UI.Style.borderField
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            MusicIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                size: 15
                color: control.primary ? UI.Style.textOnAccent : UI.Style.iconInk
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                size: 13
                font.weight: control.primary ? UI.Style.weightSemiBold : UI.Style.weightRegular
                color: control.primary ? UI.Style.textOnAccent : UI.Style.ink
            }
        }
    }
}
