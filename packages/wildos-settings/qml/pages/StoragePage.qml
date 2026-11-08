import QtQuick
import QtCore
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Storage: how full each disk is and what the Recycle Bin is holding on to, with
// the Recycle Bin emptied for real through ktrash6.
PageScroll {
    id: page

    property var volumes: System.volumes()
    property real bin: System.binSize()

    function refresh() {
        volumes = System.volumes();
        bin = System.binSize();
    }

    Connections {
        target: System
        function onBinEmptied() {
            page.refresh();
        }
    }

    Repeater {
        model: page.volumes

        UI.Card {
            id: volume
            required property var modelData
            readonly property real used: modelData.total - modelData.free
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 36

            ColumnLayout {
                id: body
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 18
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    UI.Icon {
                        name: "storage"
                        size: 22
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        UI.Label {
                            Layout.fillWidth: true
                            text: volume.modelData.name
                            size: 15
                            font.weight: Font.DemiBold
                        }

                        UI.Label {
                            Layout.fillWidth: true
                            text: System.formatBytes(volume.used) + " used of " + System.formatBytes(volume.modelData.total)
                            size: 12
                            color: UI.Style.inkMuted
                        }
                    }

                    UI.Label {
                        text: System.formatBytes(volume.modelData.free) + " free"
                        color: volume.modelData.free / volume.modelData.total < 0.1 ? UI.Style.warning : UI.Style.accentText
                    }
                }

                UI.ProgressBar {
                    Layout.fillWidth: true
                    barHeight: 8
                    value: volume.modelData.total > 0 ? volume.used / volume.modelData.total : 0
                    // Nearly full is worth a glance, so it turns Sand.
                    warning: value > 0.9
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Free up space"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    id: binRow
                    iconName: "trash"
                    title: "Recycle Bin"
                    description: page.bin > 0
                        ? System.formatBytes(page.bin) + " in files you deleted. Emptying the Recycle Bin removes them for good."
                        : "The Recycle Bin is empty."

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: page.bin > 0
                        text: "Empty Recycle Bin"
                        onClicked: emptyDialog.open()
                    }
                }

                UI.SettingRow {
                    visible: System.hasProgram("partitionmanager")
                    iconName: "storage"
                    title: "Disks and volumes"
                    description: "Format drives and change partitions"
                    external: true
                    onClicked: System.launch("partitionmanager")
                }

                // Wild OS's own File Explorer; Dolphin is hidden from people.
                UI.SettingRow {
                    visible: System.hasProgram("wildos-files")
                    iconName: "folder"
                    title: "See what is taking up space"
                    description: "Open your Home folder in Files"
                    external: true
                    onClicked: System.launch("wildos-files", [StandardPaths.writableLocation(StandardPaths.HomeLocation)])
                }
            }
        }
    }

    UI.Dialog {
        id: emptyDialog
        title: "Empty the Recycle Bin?"
        text: System.formatBytes(page.bin) + " of deleted files will be removed for good. You cannot get them back."
        buttons: [
            UI.Button {
                text: "Empty Recycle Bin"
                kind: "primary"
                onClicked: {
                    emptyDialog.close();
                    System.emptyBin();
                }
            },
            UI.Button {
                text: "Keep files"
                onClicked: emptyDialog.close()
            }
        ]
    }
}
