import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A row in the left column of People and Tasks (MailPeople.dc,
// MailTasks.dc): 32px, an icon, the name and a count. The one open has the
// neutral selected fill and semibold name, as Mail's folders do.
Item {
    id: row

    property string iconName
    property string text
    property int count: 0
    property bool current: false
    property bool showCount: true
    // Something dragged here can drop (a task on a list).
    property bool highlighted: false

    signal clicked()
    signal menuRequested()

    implicitHeight: 32
    Accessible.role: Accessible.ListItem
    Accessible.name: text

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: 6
        color: row.current || row.highlighted ? UI.Style.surfaceSelected : mouse.containsMouse ? UI.Style.surfaceHover : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 16
        spacing: 10

        MailIcon {
            name: row.iconName
            size: 16
            color: row.current ? UI.Style.iconInk : UI.Style.iconMuted
        }
        UI.Label {
            Layout.fillWidth: true
            text: row.text
            size: 13
            font.weight: row.current ? UI.Style.weightSemiBold : UI.Style.weightRegular
            elide: Text.ElideRight
        }
        UI.Label {
            visible: row.showCount && row.count > 0
            text: row.count
            size: 12
            color: UI.Style.inkMuted
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (event) {
            if (event.button === Qt.RightButton) {
                row.menuRequested();
            } else {
                row.clicked();
            }
        }
    }
}
