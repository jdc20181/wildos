import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// Every run kept in the history, newest first.
UI.Dialog {
    id: dialog

    title: qsTr("All backups")
    text: Backup.history.length === 1 ? qsTr("1 backup run on this PC.") : qsTr("%1 backup runs on this PC.").arg(Backup.history.length)

    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 420)
        clip: true
        model: Backup.history
        boundsBehavior: Flickable.StopAtBounds
        C.ScrollBar.vertical: UI.ScrollBar {}
        delegate: HistoryRow {
            required property var modelData
            width: ListView.view.width
            entry: modelData
        }
    }

    buttons: [
        UI.Button {
            kind: "primary"
            text: qsTr("Close")
            onClicked: dialog.close()
        }
    ]
}
