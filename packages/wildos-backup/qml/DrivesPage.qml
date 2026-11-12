import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// Drives: the drives backups go to, which is the default, how full each
// is, and adding another. A drive that already holds backups (from this
// PC or an earlier one) joins with the password written down for them.
Item {
    id: page

    property var app
    property string choosing
    property string removing

    readonly property var others: Drives.all.filter(d => !d.known)
    readonly property bool anyUsable: others.some(d => d.usable)

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
                title: qsTr("Drives")
                subtitle: qsTr("Backups go to your default drive when it's plugged in, or else to another of yours that is.")
            }

            Panel {
                Layout.fillWidth: true
                visible: Settings.configured
                implicitHeight: mine.implicitHeight + padding * 2

                ColumnLayout {
                    id: mine
                    width: parent.width
                    spacing: 0

                    UI.Label {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        text: qsTr("Your backup drives")
                        size: 14
                        font.weight: UI.Style.weightSemiBold
                        Accessible.role: Accessible.Heading
                    }

                    Repeater {
                        model: Settings.drives
                        delegate: Item {
                            id: driveRow
                            required property var modelData
                            readonly property var live: Drives.mine[modelData.uuid] || ({ connected: false })
                            readonly property bool isDefault: Settings.defaultDrive === modelData.uuid
                            Layout.fillWidth: true
                            implicitHeight: 64

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
                                    color: driveRow.live.connected ? UI.Style.iconInk : UI.Style.inkSubtle
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    RowLayout {
                                        spacing: 8
                                        UI.Label {
                                            text: driveRow.modelData.name
                                            size: 13.5
                                        }
                                        UI.Chip {
                                            visible: driveRow.isDefault
                                            text: qsTr("Default")
                                        }
                                    }
                                    UI.Label {
                                        Layout.fillWidth: true
                                        size: 12.5
                                        color: UI.Style.inkMuted
                                        elide: Text.ElideRight
                                        text: {
                                            const parts = [];
                                            const live = driveRow.live;
                                            if (!live.connected) {
                                                parts.push(qsTr("Not plugged in"));
                                            } else if (live.free >= 0 && live.size > 0) {
                                                parts.push(qsTr("%1 free of %2").arg(BackupStyle.size(live.free)).arg(BackupStyle.size(live.size)));
                                            } else {
                                                parts.push(qsTr("Plugged in"));
                                            }
                                            if (driveRow.modelData.lastBackup) {
                                                parts.push(qsTr("last backup %1").arg(BackupStyle.whenInSentence(new Date(driveRow.modelData.lastBackup))));
                                            } else {
                                                parts.push(qsTr("no backup yet"));
                                            }
                                            return parts.join(" · ");
                                        }
                                    }
                                }
                                UI.Button {
                                    implicitHeight: 32
                                    visible: !driveRow.isDefault
                                    text: qsTr("Make default")
                                    onClicked: Settings.makeDefault(driveRow.modelData.uuid)
                                }
                                UI.Button {
                                    implicitHeight: 32
                                    text: qsTr("Remove")
                                    onClicked: {
                                        page.removing = driveRow.modelData.uuid;
                                        removeDialog.name = driveRow.modelData.name;
                                        removeDialog.open();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: add.implicitHeight + padding * 2

                ColumnLayout {
                    id: add
                    width: parent.width
                    spacing: 0

                    UI.Label {
                        Layout.fillWidth: true
                        text: Settings.configured ? qsTr("Add another drive") : qsTr("Add a drive")
                        size: 14
                        font.weight: UI.Style.weightSemiBold
                        Accessible.role: Accessible.Heading
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 8
                        text: qsTr("Two drives, swapped now and then, means one copy is always somewhere else. Each keeps its own backups.")
                        size: 12.5
                        color: UI.Style.inkMuted
                        wrapMode: Text.Wrap
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        visible: page.others.length > 0 && !page.anyUsable
                        text: qsTr("No drive here can hold backups yet.")
                        size: 13
                        color: UI.Style.inkMuted
                        wrapMode: Text.Wrap
                    }

                    Repeater {
                        model: page.others
                        delegate: DriveChoice {
                            required property var modelData
                            Layout.fillWidth: true
                            drive: modelData
                            busy: Setup.state === "working" && page.choosing === modelData.id
                            onChosen: {
                                page.choosing = modelData.id;
                                Setup.useDrive(modelData.uuid);
                            }
                        }
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        visible: page.others.length === 0
                        text: qsTr("Plug in a USB drive or an external disk and it shows up here. The disk inside this PC isn't offered: a backup there would go with the PC.")
                        size: 13
                        color: UI.Style.inkSubtle
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    UI.Dialog {
        id: removeDialog
        property string name
        title: qsTr("Stop backing up to %1?").arg(name)
        text: qsTr("The backups already on it stay there, and you can add it again later to use them.")
        buttons: [
            UI.Button {
                text: qsTr("Cancel")
                onClicked: removeDialog.close()
            },
            UI.Button {
                kind: "primary"
                text: qsTr("Stop using it")
                onClicked: {
                    Settings.removeDrive(page.removing);
                    removeDialog.close();
                }
            }
        ]
    }
}
