import QtCore as Core
import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// What's included (AppBackup.dc): the four parts with their sizes and
// switches, Exclude a folder, and what is always left out. Windows mode is
// off until switched on: its disk is tens of gigabytes, and it is backed
// up only while Windows is shut down.
Panel {
    id: card

    readonly property var sizes: Backup.sizes

    function sized(bytes) {
        return bytes >= 0 ? BackupStyle.size(bytes) : qsTr("working out the size");
    }

    implicitHeight: column.implicitHeight + padding * 2
    Component.onCompleted: Backup.sizes.refresh(false)

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            spacing: 12

            UI.Label {
                Layout.fillWidth: true
                text: qsTr("What's included")
                size: 14
                font.weight: UI.Style.weightSemiBold
                Accessible.role: Accessible.Heading
            }
            BoardLink {
                text: qsTr("Exclude a folder")
                onClicked: folderDialog.open()
            }
        }

        IncludeRow {
            Layout.fillWidth: true
            iconName: "folder"
            title: qsTr("Your home folder")
            text: qsTr("Documents, Pictures, Music, Videos and Desktop · %1").arg(card.sized(card.sizes.home))
            checked: Settings.includeHome
            onToggled: on => Settings.includeHome = on
        }
        IncludeRow {
            Layout.fillWidth: true
            iconName: "mail"
            title: qsTr("Mail, Calendar, People and Tasks")
            text: qsTr("Everything stored on this PC · %1").arg(card.sized(card.sizes.mail))
            checked: Settings.includeMail
            onToggled: on => Settings.includeMail = on
        }
        IncludeRow {
            Layout.fillWidth: true
            iconName: "sliders"
            title: qsTr("App settings and themes")
            text: card.sized(card.sizes.settings)
            checked: Settings.includeSettings
            onToggled: on => Settings.includeSettings = on
        }
        IncludeRow {
            Layout.fillWidth: true
            visible: Backup.windowsPresent
            iconName: "monitor"
            title: qsTr("Windows mode")
            readonly property string disk: Backup.windowsBytes >= 0 ? BackupStyle.size(Backup.windowsBytes) : ""
            text: !Backup.windowsSetUp ? qsTr("Windows mode isn't set up on this PC.")
                : Settings.includeWindows ? (disk.length > 0 ? qsTr("%1, backed up on its own whenever Windows is shut down.").arg(disk)
                                                             : qsTr("Backed up on its own whenever Windows is shut down."))
                : disk.length > 0 ? qsTr("Large, %1. Back it up on its own when Windows is shut down.").arg(disk)
                : qsTr("Large. Back it up on its own when Windows is shut down.")
            textColor: Backup.windowsSetUp && !Settings.includeWindows ? BackupStyle.lateText : UI.Style.inkMuted
            checked: Settings.includeWindows
            locked: !Backup.windowsSetUp
            lockedReason: qsTr("Set Windows mode up first")
            onToggled: on => Settings.includeWindows = on
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            text: Settings.excludes.length === 0 ? qsTr("Always left out: Downloads, caches and the bin.")
                : Settings.excludes.length === 1 ? qsTr("Always left out: Downloads, caches, the bin and 1 folder you chose.")
                : qsTr("Always left out: Downloads, caches, the bin and %1 folders you chose.").arg(Settings.excludes.length)
            size: 12.5
            color: UI.Style.inkSubtle
            wrapMode: Text.Wrap
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Exclude a folder")
        currentFolder: Core.StandardPaths.writableLocation(Core.StandardPaths.HomeLocation)
        onAccepted: Settings.addExclude(decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, "")))
    }
}
