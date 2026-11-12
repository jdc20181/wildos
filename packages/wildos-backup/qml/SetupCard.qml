import QtQuick
import QtQuick.Layouts
import WildOS.Backup
import WildOS.UI as UI

// The Overview before there is a backup drive: what setting up takes, in
// four steps, and the drives plugged in now to pick from.
Panel {
    id: card

    property string choosing

    readonly property var drives: Drives.available.filter(d => !d.known)
    readonly property bool working: Setup.state === "working"
    readonly property int step: working ? 3 : drives.length > 0 ? 2 : 1

    implicitHeight: column.implicitHeight + padding * 2
    padding: 22
    sidePadding: 24

    component Step: RowLayout {
        id: stepRow

        property int number
        property string text
        property int current

        Layout.fillWidth: true
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: 24
            implicitHeight: 24
            radius: 12
            color: stepRow.number < stepRow.current ? BackupStyle.goodSoft : stepRow.number === stepRow.current ? UI.Style.accent : "transparent"
            border.width: stepRow.number > stepRow.current ? 1.5 : 0
            border.color: UI.Style.borderControl

            BackupIcon {
                anchors.centerIn: parent
                visible: stepRow.number < stepRow.current
                name: "check"
                size: 14
                strokeWidth: 2.6
                color: BackupStyle.good
            }
            UI.Label {
                anchors.centerIn: parent
                visible: stepRow.number >= stepRow.current
                text: stepRow.number
                size: 12
                font.weight: UI.Style.weightSemiBold
                color: stepRow.number === stepRow.current ? UI.Style.textOnAccent : UI.Style.inkMuted
            }
        }
        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 3
            text: stepRow.text
            size: 13.5
            color: stepRow.number > stepRow.current ? UI.Style.inkMuted : UI.Style.ink
            wrapMode: Text.Wrap
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            StatusCircle {
                Layout.alignment: Qt.AlignTop
                iconName: "shield-check"
                tone: UI.Style.accent
                fill: UI.Style.accentSoft
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                UI.Label {
                    Layout.fillWidth: true
                    text: qsTr("Set up backup")
                    size: 20
                    font.weight: UI.Style.weightBold
                    Accessible.role: Accessible.Heading
                }
                UI.Label {
                    Layout.fillWidth: true
                    text: qsTr("Keep a copy of your files on a drive of your own, so a lost, broken or stolen PC doesn't take them with it.")
                    size: 13
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: 74
            spacing: 12

            Step {
                number: 1
                current: card.step
                text: qsTr("Plug in a USB drive or an external disk. Use one with room for your files and more.")
            }
            Step {
                number: 2
                current: card.step
                text: qsTr("Pick it below. Your files on the drive stay as they are: backups go in a WildOS Backup folder.")
            }
            Step {
                number: 3
                current: card.step
                text: card.working ? Setup.step : qsTr("Backups are encrypted, with a password made for you and kept in your keyring. You're shown it once to write down.")
            }
            Step {
                number: 4
                current: card.step
                text: qsTr("Your first backup starts. After that it's every day at %1, and you can carry on working meanwhile.").arg(BackupStyle.minutes(Settings.time))
            }
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.bottomMargin: 4
            text: qsTr("Drives plugged in")
            size: 14
            font.weight: UI.Style.weightSemiBold
            Accessible.role: Accessible.Heading
        }

        Repeater {
            model: card.drives
            delegate: DriveChoice {
                required property var modelData
                Layout.fillWidth: true
                drive: modelData
                busy: card.working && card.choosing === modelData.uuid
                onChosen: {
                    card.choosing = modelData.uuid;
                    Setup.useDrive(modelData.uuid);
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: card.drives.length === 0
            implicitHeight: waiting.implicitHeight + 32
            radius: 8
            color: "transparent"
            border.width: 1
            border.color: BackupStyle.fieldEdge

            RowLayout {
                id: waiting
                anchors.centerIn: parent
                spacing: 10

                UI.Spinner {
                    size: 14
                    color: UI.Style.inkMuted
                }
                UI.Label {
                    text: qsTr("Waiting for a drive. Plug one in and it shows up here.")
                    size: 13
                    color: UI.Style.inkMuted
                }
            }
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 12
            text: qsTr("The disk inside this PC isn't offered: a backup there would go with the PC.")
            size: 12.5
            color: UI.Style.inkSubtle
            wrapMode: Text.Wrap
        }
    }
}
