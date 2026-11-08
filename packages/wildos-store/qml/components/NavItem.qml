import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A Store section in the left column: 38px, 17px line icon, 13px label. The
// current one gets the selected fill, a short accent bar and an accent icon.
T.AbstractButton {
    id: item

    property string iconName
    property bool current: false
    property int badge: 0

    implicitHeight: Math.max(38, Math.ceil(label.implicitHeight) + 16)
    // Read by the window, which widens the column to fit the longest name.
    implicitWidth: 12 + 17 + 12 + Math.ceil(label.implicitWidth) + 12 + (badge > 0 ? badgePill.width + 12 : 0)
    width: parent ? parent.width : implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.PageTab
    Accessible.name: badge > 0 ? text + ", " + badge : text
    Accessible.selected: current

    background: Rectangle {
        radius: UI.Style.radiusSm
        color: item.down ? UI.Style.surfacePressed
             : item.hovered ? UI.Style.surfaceHover
             : item.current ? UI.Style.surfaceSelected : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        Rectangle {
            visible: item.current
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 16
            radius: 2
            color: UI.Style.accent
        }

        UI.FocusRing {
            visible: item.visualFocus
            targetRadius: UI.Style.radiusSm
        }
    }

    contentItem: Item {
        UI.Icon {
            id: icon
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            name: item.iconName
            size: 17
            strokeWidth: item.current ? 1.9 : 1.8
            color: item.current ? UI.Style.accent : UI.Style.iconInk
        }

        UI.Label {
            id: label
            anchors.left: icon.right
            anchors.leftMargin: 12
            anchors.right: badgePill.visible ? badgePill.left : parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: item.text
            font.weight: item.current ? Font.DemiBold : Font.Normal
        }

        Rectangle {
            id: badgePill
            visible: item.badge > 0
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(20, badgeText.implicitWidth + 12)
            height: 20
            radius: 10
            color: UI.Style.accent

            UI.Label {
                id: badgeText
                anchors.centerIn: parent
                text: item.badge > 99 ? "99+" : item.badge
                size: 11
                font.weight: Font.Bold
                color: UI.Style.textOnAccent
            }
        }
    }
}
