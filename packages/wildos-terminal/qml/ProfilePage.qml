pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Terminal
import WildOS.UI as UI

// One profile: its name, what it runs, where it starts and its icon.
PageScroll {
    id: page

    required property string profileId

    signal openProfile(string id)

    readonly property var profile: Config.profile(profileId)
    readonly property bool isDefault: Config.defaultProfile === profileId
    readonly property var icons: ["terminal", "code", "server", "shield", "box", "globe", "bolt", "star", "folder"]

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                title: "Name"
                description: "Shown on the tab and in the new tab menu."

                Field {
                    anchors.verticalCenter: parent.verticalCenter
                    value: page.profile.name || ""
                    placeholderText: "Profile name"
                    onCommitted: text => {
                        if (text.trim().length > 0) {
                            Config.setProfileValue(page.profileId, "name", text.trim());
                        }
                    }
                }
            }

            UI.SettingRow {
                title: "Command"
                description: "The program to run, with its options. Leave it empty for your usual shell."

                Field {
                    anchors.verticalCenter: parent.verticalCenter
                    monospace: true
                    value: page.profile.command || ""
                    placeholderText: Config.defaultShell
                    onCommitted: text => Config.setProfileValue(page.profileId, "command", text.trim())
                }
            }

            UI.SettingRow {
                title: "Starting folder"
                description: "Where new tabs with this profile open. Leave it empty for your home folder."

                Field {
                    anchors.verticalCenter: parent.verticalCenter
                    monospace: true
                    value: page.profile.directory || ""
                    placeholderText: "~"
                    onCommitted: text => Config.setProfileValue(page.profileId, "directory", text.trim())
                }
            }

            UI.SettingRow {
                title: "Icon"
                description: "Shown on the tab."

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Repeater {
                        model: page.icons

                        // One icon of a set, chosen like a choice card: accent
                        // edge and fill when it is the profile's.
                        T.AbstractButton {
                            id: choice
                            required property string modelData
                            readonly property bool selected: page.profile.icon === modelData
                            width: 34
                            height: 34
                            padding: 0
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: modelData
                            Accessible.checked: selected
                            onClicked: Config.setProfileValue(page.profileId, "icon", modelData)

                            background: Rectangle {
                                radius: UI.Style.radiusSm
                                color: choice.selected ? UI.Style.accentSelected : choice.down ? UI.Style.surfacePressed : choice.hovered ? UI.Style.surfaceHover : "transparent"
                                border.width: choice.selected ? 1.5 : 0
                                border.color: UI.Style.accent
                                Behavior on color {
                                    ColorAnimation { duration: UI.Style.hoverDuration }
                                }

                                UI.FocusRing {
                                    visible: choice.visualFocus
                                    targetRadius: UI.Style.radiusSm
                                }
                            }

                            contentItem: Item {
                                UI.Icon {
                                    anchors.centerIn: parent
                                    name: choice.modelData
                                    size: 18
                                    strokeWidth: 1.8
                                    color: choice.selected ? UI.Style.accent : UI.Style.iconInk
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "star"
                title: page.isDefault ? "This is the default profile" : "Make this the default profile"
                description: "The default profile opens in new windows and with Ctrl+Shift+T."

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !page.isDefault
                    text: "Make default"
                    onClicked: Config.defaultProfile = page.profileId
                }
            }

            UI.SettingRow {
                iconName: "plus"
                title: "Open a tab with this profile"

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Open"
                    onClicked: host.newTab(page.profileId)
                }
            }

            UI.SettingRow {
                iconName: "duplicate"
                title: "Duplicate this profile"
                description: "A copy to change without losing this one."

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Duplicate"
                    onClicked: {
                        const copy = Config.duplicateProfile(page.profileId);
                        if (copy.length > 0) {
                            page.openProfile(copy);
                        }
                    }
                }
            }

            UI.SettingRow {
                iconName: "trash"
                title: "Delete this profile"
                description: Config.profiles.length > 1 ? "Tabs already open with it keep running."
                                                        : "Terminal needs at least one profile, so the last one stays."

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Delete"
                    danger: true
                    enabled: Config.profiles.length > 1
                    onClicked: Config.removeProfile(page.profileId)
                }
            }
        }
    }
}
