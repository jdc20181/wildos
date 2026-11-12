import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A heading in the left column of People and Tasks, Groups or Lists, that
// folds its rows away.
Item {
    id: heading

    property string text
    property bool expanded: true

    implicitHeight: 36
    Accessible.role: Accessible.Button
    Accessible.name: text

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 8
        MailIcon {
            name: heading.expanded ? "chevron-down" : "chevron-right"
            size: 13
            color: UI.Style.iconMuted
        }
        UI.Label {
            Layout.fillWidth: true
            text: heading.text
            size: 13
            font.weight: UI.Style.weightSemiBold
            elide: Text.ElideRight
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: heading.expanded = !heading.expanded
    }
}
