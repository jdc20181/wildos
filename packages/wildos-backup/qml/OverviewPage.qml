import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// Overview (AppBackup.dc): how the backups stand, the drive, the next
// backup and the encryption, what's included and what's kept, with the
// recent runs down the right. Before a drive is set up, the setup steps.
Item {
    id: page

    property var app

    readonly property var drive: Drives.mine[Settings.defaultDrive] || ({ connected: false })

    Component.onCompleted: Backup.refreshWindows()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: column.implicitHeight + 44
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            C.ScrollBar.vertical: UI.ScrollBar {}

            ColumnLayout {
                id: column
                x: 24
                y: 22
                width: flick.width - 48
                spacing: 14

                UI.Label {
                    Layout.fillWidth: true
                    text: qsTr("Overview")
                    size: 26
                    font.weight: UI.Style.weightBold
                    Accessible.role: Accessible.Heading
                }

                SetupCard {
                    Layout.fillWidth: true
                    visible: !Settings.configured
                }

                StatusCard {
                    Layout.fillWidth: true
                    visible: Settings.configured
                    app: page.app
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: Settings.configured
                    spacing: 12

                    InfoCard {
                        Layout.preferredWidth: (column.width - 24) / 3
                        Layout.fillHeight: true
                        iconName: "drive"
                        title: qsTr("Backup drive")
                        readonly property bool hasRoom: page.drive.connected && page.drive.size > 0 && page.drive.free >= 0
                        text: {
                            const name = page.drive.kind || page.drive.name || qsTr("Your drive");
                            if (!page.drive.connected) {
                                return qsTr("%1 · not plugged in").arg(page.drive.name || name);
                            }
                            return hasRoom ? qsTr("%1 · %2 free of %3").arg(name).arg(BackupStyle.size(page.drive.free)).arg(BackupStyle.size(page.drive.size)) : name;
                        }
                        fill: hasRoom ? 1 - page.drive.free / page.drive.size : -1
                    }
                    InfoCard {
                        Layout.preferredWidth: (column.width - 24) / 3
                        Layout.fillHeight: true
                        iconName: "schedule"
                        title: qsTr("Next backup")
                        text: Settings.scheduled && !isNaN(Backup.nextRun)
                            ? qsTr("%1, %2").arg(BackupStyle.next(Backup.nextRun)).arg(BackupStyle.days(Settings.days))
                            : qsTr("Only when you back up now")
                    }
                    InfoCard {
                        Layout.preferredWidth: (column.width - 24) / 3
                        Layout.fillHeight: true
                        iconName: "lock"
                        title: qsTr("Encrypted")
                        text: qsTr("Password saved in your keyring")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    IncludedCard {
                        Layout.preferredWidth: (column.width - 14) * 1.4 / 2.4
                        Layout.alignment: Qt.AlignTop
                    }
                    KeepCard {
                        Layout.preferredWidth: (column.width - 14) / 2.4
                        Layout.alignment: Qt.AlignTop
                    }
                }
            }
        }

        RecentColumn {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
        }
    }
}
