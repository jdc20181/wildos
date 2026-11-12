import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The boards' 32px buttons: 16px either side, radius 6, 13px text, an
// optional 15px icon. Secondary sits on a 0.06 fill with a 0.10 edge;
// primary is accent filled in semibold.
T.Button {
    id: control

    property bool primary: false
    property string iconName

    implicitWidth: row.implicitWidth + 32
    implicitHeight: 32
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1 : 0.5
    Accessible.name: text

    background: Rectangle {
        radius: 6
        color: control.primary ? (control.down ? UI.Style.accentPressed : control.hovered ? UI.Style.accentHover : UI.Style.accent)
                               : (control.down ? UI.Style.tone(0.04) : control.hovered ? UI.Style.tone(0.09) : UI.Style.tone(0.06))
        border.width: control.primary ? 0 : 1
        border.color: Look.fieldEdge

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: 6
        }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            ReportIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
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
