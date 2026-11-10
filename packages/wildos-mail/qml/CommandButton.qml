import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A button in Mail's command bar (MailMicrosoft.dc): 34px, icon and label
// in 13px ink, no fill until hovered. Set `menu` to open it on click.
T.AbstractButton {
    id: control

    property string iconName
    property bool iconOnly: false
    property string tip: text
    property UI.Menu menu: null

    implicitHeight: 34
    implicitWidth: iconOnly ? 34 : row.implicitWidth + 20
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.45
    Accessible.name: tip

    onClicked: if (menu) menu.popup(control, 0, control.height + 4)

    background: Rectangle {
        radius: control.iconOnly ? UI.Style.radiusSm : 6
        color: control.down ? UI.Style.surfacePressed : control.hovered ? UI.Style.surfaceHover : "transparent"
        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 7

            MailIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                size: 17
                color: UI.Style.accent
                visible: control.iconName.length > 0
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.iconOnly
                text: control.text
                color: UI.Style.ink
                font.family: UI.Style.fontFamily
                font.pixelSize: UI.Style.px(13)
            }
        }
    }

    UI.Tooltip {
        visible: control.iconOnly && control.hovered && control.tip.length > 0
        text: control.tip
    }
}
