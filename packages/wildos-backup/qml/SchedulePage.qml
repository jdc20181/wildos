import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// Schedule: whether backups run by themselves, at what time and on which
// days, and how a missed one is caught up.
Item {
    id: page

    property var app

    readonly property var times: {
        const list = [];
        for (let m = 0; m < 24 * 60; m += 30) {
            list.push({ value: m, text: BackupStyle.minutes(m) });
        }
        if (Settings.time % 30 !== 0) {
            list.push({ value: Settings.time, text: BackupStyle.minutes(Settings.time) });
            list.sort((a, b) => a.value - b.value);
        }
        return list;
    }

    function toggleDay(day) {
        const days = Settings.days.slice();
        const i = days.indexOf(day);
        if (i >= 0) {
            days.splice(i, 1);
        } else {
            days.push(day);
        }
        Settings.days = days;
    }

    component SettingLine: RowLayout {
        id: line

        property string title
        property string text
        default property alias control: holder.data

        Layout.fillWidth: true
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: line.title
                size: 13.5
            }
            UI.Label {
                Layout.fillWidth: true
                visible: line.text.length > 0
                text: line.text
                size: 12.5
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
            }
        }
        Item {
            id: holder
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + 44
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        C.ScrollBar.vertical: UI.ScrollBar {}

        ColumnLayout {
            id: column
            x: 24
            y: 22
            width: Math.min(flick.width - 48, 760)
            spacing: 14

            PageHeading {
                Layout.fillWidth: true
                title: qsTr("Schedule")
                subtitle: Settings.scheduled && !isNaN(Backup.nextRun)
                    ? qsTr("Next backup: %1.").arg(BackupStyle.next(Backup.nextRun))
                    : qsTr("Backups run only when you press Back up now.")
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: when.implicitHeight + padding * 2

                ColumnLayout {
                    id: when
                    width: parent.width
                    spacing: 16

                    SettingLine {
                        title: qsTr("Back up automatically")
                        text: qsTr("Backups run in the background while you work.")

                        UI.Toggle {
                            checked: Settings.scheduled
                            Accessible.name: qsTr("Back up automatically")
                            onToggled: Settings.scheduled = checked
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: BackupStyle.hairline
                    }

                    SettingLine {
                        title: qsTr("Time")
                        text: qsTr("If the PC is off or asleep then, the backup runs as soon as it's back.")
                        enabled: Settings.scheduled
                        opacity: enabled ? 1 : 0.5

                        UI.ComboBox {
                            width: 120
                            height: 32
                            model: page.times
                            currentIndex: page.times.findIndex(o => o.value === Settings.time)
                            Accessible.name: qsTr("Time")
                            onActivated: index => Settings.time = page.times[index].value
                        }
                    }

                    SettingLine {
                        title: qsTr("Days")
                        text: Settings.days.length === 0 ? qsTr("Pick at least one day.") : qsTr("Backs up %1.").arg(BackupStyle.days(Settings.days))
                        enabled: Settings.scheduled
                        opacity: enabled ? 1 : 0.5

                        Row {
                            spacing: 6

                            Repeater {
                                model: [1, 2, 3, 4, 5, 6, 7]
                                delegate: DayChip {
                                    required property int modelData
                                    day: modelData
                                    checked: Settings.days.indexOf(modelData) >= 0
                                    onClicked: page.toggleDay(modelData)
                                }
                            }
                        }
                    }
                }
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: missed.implicitHeight + padding * 2

                RowLayout {
                    id: missed
                    width: parent.width
                    spacing: 12

                    BackupIcon {
                        Layout.alignment: Qt.AlignTop
                        name: "drive"
                        size: 18
                        color: UI.Style.inkMuted
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        UI.Label {
                            Layout.fillWidth: true
                            text: qsTr("When the drive isn't plugged in")
                            size: 13.5
                            font.weight: UI.Style.weightSemiBold
                        }
                        UI.Label {
                            Layout.fillWidth: true
                            text: qsTr("Backup waits, and catches up as soon as you plug the drive back in. The Overview shows a backup is waiting. You don't need to leave the drive plugged in all the time.")
                            size: 12.5
                            color: UI.Style.inkMuted
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}
