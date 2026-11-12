import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// Keep (AppBackup.dc): how long each kind of backup stays on the drive.
// Every backup for 2 days, a daily one for 2 weeks, a weekly one for 3
// months and a monthly one for a year, unless changed here.
Panel {
    id: card

    implicitHeight: column.implicitHeight + padding * 2

    component KeepRow: RowLayout {
        id: keepRow

        property string label
        property var choices: []
        property var value

        signal chosen(var value)

        Layout.fillWidth: true
        spacing: 12

        UI.Label {
            // One width for all four, so the drop-downs line up.
            Layout.preferredWidth: Math.min(110, (card.width - card.sidePadding * 2) * 0.4)
            text: keepRow.label
            size: 13
            color: UI.Style.inkMuted
            elide: Text.ElideRight
        }
        UI.ComboBox {
            Layout.fillWidth: true
            Layout.maximumWidth: 170
            Layout.minimumWidth: 100
            Layout.preferredHeight: 32
            model: keepRow.choices
            currentIndex: keepRow.choices.findIndex(o => o.value === keepRow.value)
            Accessible.name: keepRow.label
            onActivated: index => keepRow.chosen(keepRow.choices[index].value)
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 10

        UI.Label {
            Layout.fillWidth: true
            Layout.bottomMargin: 2
            text: qsTr("Keep")
            size: 14
            font.weight: UI.Style.weightSemiBold
            Accessible.role: Accessible.Heading
        }
        KeepRow {
            label: qsTr("Every backup")
            choices: BackupStyle.keepWithin
            value: Settings.keepWithin
            onChosen: v => Settings.keepWithin = v
        }
        KeepRow {
            label: qsTr("Daily")
            choices: BackupStyle.keepDaily
            value: Settings.keepDaily
            onChosen: v => Settings.keepDaily = v
        }
        KeepRow {
            label: qsTr("Weekly")
            choices: BackupStyle.keepWeekly
            value: Settings.keepWeekly
            onChosen: v => Settings.keepWeekly = v
        }
        KeepRow {
            label: qsTr("Monthly")
            choices: BackupStyle.keepMonthly
            value: Settings.keepMonthly
            onChosen: v => Settings.keepMonthly = v
        }
        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: qsTr("Oldest backups go first when the drive is nearly full.")
            size: 12.5
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }
    }
}
