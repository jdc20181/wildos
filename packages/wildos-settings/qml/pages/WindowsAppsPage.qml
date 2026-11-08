import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Windows apps, laid out as WinAppsSettings.dc.html. The App Bridge runs
// each Windows app in a Wine prefix of its own; this page holds its two
// settings, lists the apps it has set up and can remove them. Until the
// bridge is installed the switches are held back and say so.
PageScroll {
    id: page

    property var removing: null

    UI.Label {
        Layout.fillWidth: true
        Layout.maximumWidth: 720
        text: "Windows apps run in their own sandbox. Each one only reaches what you allow, and you can change that at any time."
        size: 14
        lineHeightMode: Text.FixedHeight
        lineHeight: 22 * UI.Style.textScale
        color: UI.Style.inkStatus
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                title: "Windows app support"
                description: WindowsApps.installed
                    ? "Runs Windows apps in a sandbox. Turn off to block every Windows app."
                    : "The part of Wild OS that runs Windows apps is not installed on this computer yet."
                enabled: WindowsApps.installed
                status: (WindowsApps.installed && WindowsApps.enabled) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: WindowsApps.installed && WindowsApps.enabled
                    Accessible.name: "Windows app support"
                    onToggled: WindowsApps.setEnabled(checked)
                }
            }

            UI.SettingRow {
                title: "Starting permissions for new apps"
                description: "What a newly installed Windows app can reach until you change it"

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Starting permissions"
                    enabled: WindowsApps.installed && WindowsApps.enabled
                    model: [
                        { value: "documents-offline", text: "Documents only, no internet" },
                        { value: "documents-online", text: "Documents and internet" },
                        { value: "own-offline", text: "Its own folder only, no internet" }
                    ]
                    readonly property var choice: WindowsApps.startingPermissions
                    currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                    onActivated: index => WindowsApps.setStartingPermissions(model[index].value)
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Installed Windows apps"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: WindowsApps.apps

                    UI.SettingRow {
                        id: app
                        required property var modelData
                        minHeight: 68
                        iconTile: true
                        iconName: "windowsapps"
                        themeIcon: modelData.icon
                        title: modelData.title

                        UI.Button {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Uninstall"
                            onClicked: {
                                page.removing = app.modelData;
                                appDialog.open();
                            }
                        }

                        below: Repeater {
                            model: app.modelData.permissions

                            UI.Chip {
                                required property var modelData
                                text: modelData.text
                                granted: modelData.granted
                            }
                        }
                    }
                }

                UI.SettingRow {
                    visible: WindowsApps.apps.length === 0
                    title: "No Windows apps yet"
                    description: WindowsApps.installed
                        ? "Open a Windows installer, an .exe or .msi file, and Wild OS offers to set it up here."
                        : "Once Windows app support is installed, apps you set up appear here."
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "When a Windows app won't run"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                // Windows mode, the background Windows VM, has a page of
                // its own (WinModeSettings.dc).
                UI.SettingRow {
                    title: "Windows mode"
                    description: WindowsMode.state === "ready" ? "Set up. Office and other Windows programs open in their own windows."
                        : WindowsMode.state === "setup" ? "Setting up in the background"
                        : "Runs the hardest apps in a real copy of Windows, using the licence built into this PC"
                    chevron: true
                    onClicked: Navigator.open("windowsmode")
                }
            }
        }
    }

    UI.Dialog {
        id: appDialog
        title: page.removing ? "Uninstall " + page.removing.title + "?" : ""
        text: "Uninstalling removes the app and everything saved inside its sandbox. Files it saved to your Documents stay where they are. You can bring it back from the Recycle Bin."
        buttons: [
            UI.Button {
                text: "Uninstall"
                kind: "primary"
                onClicked: {
                    WindowsApps.uninstall(page.removing.name);
                    appDialog.close();
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: appDialog.close()
            }
        ]
    }
}
