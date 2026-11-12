import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// The right-hand column of the Overview (AppBackup.dc): the last six runs
// and See them all.
Item {
    id: column

    readonly property var recent: Backup.history.slice(0, 6)

    Rectangle {
        width: 1
        height: parent.height
        color: BackupStyle.hairline
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 13
        anchors.rightMargin: 12
        anchors.topMargin: 22
        anchors.bottomMargin: 22
        spacing: 0

        UI.Label {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.bottomMargin: 8
            text: qsTr("Recent backups")
            size: 14
            font.weight: UI.Style.weightSemiBold
            Accessible.role: Accessible.Heading
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 4
            visible: column.recent.length === 0
            text: qsTr("Backups you make show up here.")
            size: 12.5
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }

        Repeater {
            model: column.recent
            delegate: HistoryRow {
                required property var modelData
                Layout.fillWidth: true
                entry: modelData
            }
        }

        BoardLink {
            Layout.leftMargin: 12
            Layout.topMargin: 10
            visible: Backup.history.length > 0
            text: qsTr("See them all")
            onClicked: historyDialog.open()
        }

        Item {
            Layout.fillHeight: true
        }
    }

    HistoryDialog {
        id: historyDialog
    }
}
