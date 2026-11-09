import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Desktops, after Windows 11's virtual desktops: the list of desktops with
// adding, renaming and removing. The desktops are KWin's, changed over its
// D-Bus interface, so Task View and the shortcuts follow at once. Which
// windows the taskbar and Alt+Tab show sits on Multitasking, as in Windows.
PageScroll {
    id: page

    readonly property bool several: Desktops.desktops.length > 1

    function openMenu(button, desktop) {
        renameDialog.desktop = desktop;
        desktopMenu.entries = [
            { text: "Switch to this desktop", icon: "check", enabled: desktop.id !== Desktops.current, run: () => Desktops.switchTo(desktop.id) },
            { separator: true },
            { text: "Rename", icon: "rename", run: () => renameDialog.open() },
            { text: "Remove", icon: "delete", danger: true, enabled: page.several, run: () => Desktops.remove(desktop.id) }
        ];
        desktopMenu.popup(button, 0, button.height + 4);
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            SectionTitle {
                Layout.fillWidth: true
                text: "Your desktops"
            }

            UI.Button {
                text: "Add a desktop"
                iconName: "plus"
                enabled: Desktops.available && Desktops.desktops.length < Desktops.maximum
                onClicked: Desktops.add()
            }
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: Desktops.desktops

                    UI.SettingRow {
                        id: desktopRow
                        required property var modelData
                        iconName: "desktops"
                        title: modelData.name
                        status: modelData.id === Desktops.current ? "In use" : ""
                        statusColor: UI.Style.accentText

                        UI.IconButton {
                            id: moreButton
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "more"
                            tip: "Options for " + desktopRow.modelData.name
                            onClicked: page.openMenu(moreButton, desktopRow.modelData)
                        }
                    }
                }

                UI.SettingRow {
                    visible: !Desktops.available
                    iconName: "desktops"
                    title: "Desktops aren't available right now"
                    description: "The window manager isn't answering. Try again after signing out and back in."
                }
            }
        }

        UI.Label {
            Layout.fillWidth: true
            text: "Removing a desktop moves its windows to the desktop next to it."
            color: UI.Style.inkMuted
            size: 12
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }

    UI.Menu {
        id: desktopMenu
    }

    UI.Dialog {
        id: renameDialog

        property var desktop: null
        readonly property string trimmed: nameField.text.trim()

        title: "Rename desktop"
        onAboutToShow: {
            nameField.text = desktop ? desktop.name : "";
            nameField.selectAll();
            nameField.forceActiveFocus();
        }

        UI.TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Desktop name"
            maximumLength: 64
            onAccepted: renameButton.clicked()
        }

        buttons: [
            UI.Button {
                id: renameButton
                text: "Rename"
                kind: "primary"
                enabled: renameDialog.desktop !== null && renameDialog.trimmed.length > 0 && renameDialog.trimmed !== renameDialog.desktop.name
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    Desktops.rename(renameDialog.desktop.id, renameDialog.trimmed);
                    renameDialog.close();
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: renameDialog.close()
            }
        ]
    }
}
