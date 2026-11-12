import QtQuick
import WildOS.UI as UI

// A Wild OS button that can carry one of Mail's own icons (MailIcons), such
// as New contact's person with a plus or New event's calendar, which the
// system set doesn't have.
UI.Button {
    id: control

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            MailIcon {
                visible: control.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                size: control.iconSize
                strokeWidth: control.iconStroke
                color: control.iconColor
            }

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                size: control.fontSize
                font.weight: control.primary ? UI.Style.weightSemiBold : UI.Style.weightMedium
                color: control.textColor
            }
        }
    }
}
