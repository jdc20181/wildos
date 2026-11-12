import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The Sign pane's buttons (AppPdf.dc): 32px, radius 6, a 15px icon 8px
// before 13px text. Secondary ones have the control fill and edge; the
// primary one (Save a signed copy) is filled with the accent.
T.Button {
    id: control

    property string iconName
    property bool primary: false

    readonly property bool lit: hovered && enabled

    implicitHeight: 32
    implicitWidth: row.implicitWidth + 32
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5
    Accessible.name: text
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: UI.Style.radiusXs
        color: control.primary ? (control.down ? UI.Style.accentPressed : control.lit ? UI.Style.accentHover : UI.Style.accent)
            : control.down ? UI.Style.surfacePressed : control.lit ? UI.Style.surfaceHover : UI.Style.surfaceControl
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
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            PdfIcon {
                visible: control.iconName.length > 0
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
