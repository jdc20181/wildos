import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// A drive that's plugged in: its name, kind and room, and Use this drive.
// One Backup can't use is greyed, says why, and offers Format this drive
// when formatting would fix it.
Item {
    id: row

    property var drive: ({})
    property bool busy: false
    property string actionText: qsTr("Use this drive")

    readonly property bool usable: drive.usable !== false

    signal chosen()

    implicitHeight: Math.max(56, details.implicitHeight + 20)

    Rectangle {
        width: parent.width
        height: 1
        color: BackupStyle.hairline
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        BackupIcon {
            Layout.alignment: row.usable ? Qt.AlignVCenter : Qt.AlignTop
            Layout.topMargin: row.usable ? 0 : 11
            name: "drive"
            size: 18
            color: row.usable ? UI.Style.inkMuted : UI.Style.inkSubtle
        }
        ColumnLayout {
            id: details
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.drive.name || ""
                size: 13.5
                color: row.usable ? UI.Style.ink : UI.Style.inkMuted
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
                color: row.usable ? UI.Style.inkMuted : UI.Style.inkSubtle
                elide: Text.ElideRight
            }
            UI.Label {
                Layout.fillWidth: true
                visible: !row.usable
                text: row.drive.reasonText || ""
                size: 12.5
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
            }
        }
        UI.Button {
            implicitHeight: 32
            visible: row.usable
            text: row.actionText
            busy: row.busy
            enabled: Setup.state !== "working"
            onClicked: row.chosen()
        }
        UI.Button {
            implicitHeight: 32
            visible: !row.usable && row.drive.canFormat === true
            text: qsTr("Format this drive")
            enabled: Setup.state !== "working"
            onClicked: Drives.formatDrive(row.drive.device)
        }
    }
}
