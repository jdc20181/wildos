import QtQuick
import WildOS.UI as UI

// One step of the path bar: the archive (with its 16px icon) or a folder
// in it. The step being shown is semibold.
Rectangle {
    id: crumb

    property string text
    property bool showIcon: false
    property bool current: false

    signal clicked()

    implicitWidth: row.implicitWidth + 8
    implicitHeight: 26
    radius: 6
    color: mouse.containsMouse && !crumb.current ? ArchiveStyle.rowHover : "transparent"

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Image {
            anchors.verticalCenter: parent.verticalCenter
            visible: crumb.showIcon
            width: 16
            height: 16
            source: ArchiveStyle.icon
            sourceSize.width: 32
            sourceSize.height: 32
        }
        UI.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: crumb.text
            size: 13
            font.weight: crumb.current && !crumb.showIcon ? UI.Style.weightSemiBold : UI.Style.weightRegular
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: !crumb.current
        cursorShape: Qt.PointingHandCursor
        onClicked: crumb.clicked()
        Accessible.role: Accessible.Button
        Accessible.name: crumb.text
    }
}
