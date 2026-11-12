import QtQuick
import WildOS.UI as UI

// A column heading of the table, 12px and faint, with an arrow when the
// rows are sorted by it.
Item {
    id: heading

    property string text
    property string key
    property string sortKey
    property bool ascending: true
    property bool alignRight: false
    property real inset: 0

    signal clicked()

    implicitHeight: 32
    height: parent ? parent.height : 32

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: heading.alignRight ? parent.right : undefined
        anchors.left: heading.alignRight ? undefined : parent.left
        anchors.leftMargin: heading.inset
        spacing: 4
        layoutDirection: heading.alignRight ? Qt.RightToLeft : Qt.LeftToRight

        UI.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: heading.text
            size: 12
            color: mouse.containsMouse ? UI.Style.inkMuted : UI.Style.inkSubtle
        }
        ArchiveIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: heading.sortKey === heading.key
            name: heading.ascending ? "sort-up" : "sort-down"
            size: 11
            color: UI.Style.inkSubtle
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: heading.clicked()
        Accessible.role: Accessible.ColumnHeader
        Accessible.name: heading.text
    }
}
