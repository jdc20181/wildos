import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The heading of a list page (Songs, Albums, Artists, Folders, Search):
// a 28px title, a quiet count under it, and the page's own buttons on the
// right.
Item {
    id: heading

    property string title
    property string subtitle
    default property alias actions: actionRow.data

    implicitHeight: column.implicitHeight + 28 + 18

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: actionRow.left
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 16
        anchors.topMargin: 28
        spacing: 4

        UI.Label {
            Layout.fillWidth: true
            text: heading.title
            size: 28
            font.weight: UI.Style.weightBold
            Accessible.role: Accessible.Heading
        }
        UI.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: heading.subtitle
            size: 14
            color: UI.Style.inkMuted
        }
    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.rightMargin: 28
        anchors.bottom: column.bottom
        spacing: 8
    }
}
