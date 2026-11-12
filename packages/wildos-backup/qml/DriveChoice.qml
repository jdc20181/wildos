import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// A drive that's plugged in and could take backups: its name, kind and
// room, and Use this drive.
Item {
    id: row

    property var drive: ({})
    property bool busy: false
    property string actionText: qsTr("Use this drive")

    signal chosen()

    implicitHeight: 56

    Rectangle {
        width: parent.width
        height: 1
        color: BackupStyle.hairline
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        BackupIcon {
            name: "drive"
            size: 18
            color: UI.Style.inkMuted
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.drive.name || ""
                size: 13.5
                elide: Text.ElideRight
            }
            UI.Label {
                Layout.fillWidth: true
                text: {
                    const parts = [row.drive.kind];
                    if (row.drive.free >= 0 && row.drive.size > 0) {
                        parts.push(qsTr("%1 free of %2").arg(BackupStyle.size(row.drive.free)).arg(BackupStyle.size(row.drive.size)));
                    } else if (row.drive.size > 0) {
                        parts.push(BackupStyle.size(row.drive.size));
                    }
                    return parts.join(" · ");
                }
                size: 12.5
                color: UI.Style.inkMuted
                elide: Text.ElideRight
            }
        }
        UI.Button {
            implicitHeight: 32
            text: row.actionText
            busy: row.busy
            enabled: Setup.state !== "working"
            onClicked: row.chosen()
        }
    }
}
