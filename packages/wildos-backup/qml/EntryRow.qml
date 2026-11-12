import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// A file or folder in a backup: a tick box to pick it, its name, size and
// when it was last changed. A folder opens with a click on its name or a
// double click anywhere on the row.
Rectangle {
    id: row

    property var entry: ({})
    property bool showPath: false
    property string pathText

    readonly property bool picked: Restore.selected.indexOf(entry.path) >= 0

    implicitHeight: showPath ? 50 : 38
    color: picked ? BackupStyle.rowSelected : mouse.containsMouse ? BackupStyle.rowHover : "transparent"
    Accessible.role: Accessible.ListItem
    Accessible.name: entry.name
    Accessible.selected: picked

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: Restore.toggle(row.entry.path)
        onDoubleClicked: {
            if (row.entry.dir) {
                Restore.openFolder(row.entry.path);
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 12

        UI.CheckBox {
            checked: row.picked
            Accessible.name: qsTr("Pick %1").arg(row.entry.name)
            onToggled: Restore.toggle(row.entry.path)
        }
        BackupIcon {
            name: row.entry.dir ? "folder" : "file"
            size: 16
            color: UI.Style.inkMuted
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            UI.Label {
                Layout.fillWidth: true
                text: row.entry.name
                size: 13
                elide: Text.ElideMiddle
                color: row.entry.dir && nameMouse.containsMouse ? BackupStyle.link : UI.Style.ink
                font.underline: row.entry.dir && nameMouse.containsMouse

                MouseArea {
                    id: nameMouse
                    anchors.fill: parent
                    enabled: row.entry.dir === true
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Restore.openFolder(row.entry.path)
                }
            }
            UI.Label {
                Layout.fillWidth: true
                visible: row.showPath
                text: row.pathText
                size: 11.5
                color: UI.Style.inkSubtle
                elide: Text.ElideMiddle
            }
        }
        UI.Label {
            Layout.preferredWidth: 90
            text: row.entry.dir ? "" : BackupStyle.size(row.entry.size)
            size: 12.5
            color: UI.Style.inkMuted
            horizontalAlignment: Text.AlignRight
        }
        UI.Label {
            Layout.preferredWidth: 150
            text: row.entry.mtime ? BackupStyle.when(row.entry.mtime) : ""
            size: 12.5
            color: UI.Style.inkMuted
            elide: Text.ElideRight
        }
    }
}
