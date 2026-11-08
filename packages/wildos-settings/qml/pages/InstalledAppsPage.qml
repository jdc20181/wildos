import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Installed apps: everything that shows in Start, with Uninstall for the
// apps that can safely go. Store apps are removed with flatpak, others with
// PackageKit, which asks for the administrator password.
PageScroll {
    id: page

    property string message
    property bool messageIsError: false

    AppsModel {
        id: apps
        onUninstalled: name => {
            page.messageIsError = false;
            page.message = name + " was uninstalled.";
        }
        onUninstallFailed: (name, reason) => {
            page.messageIsError = true;
            page.message = name + " was not uninstalled. " + (reason.length > 0 ? reason : "Try again from the Store.");
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 16

        UI.SearchField {
            Layout.preferredWidth: 320
            placeholderText: "Search apps"
            onTextChanged: apps.filter = text
        }

        UI.Label {
            Layout.fillWidth: true
            text: Format.plural(apps.count, "app", "apps")
            color: UI.Style.inkMuted
            horizontalAlignment: Text.AlignRight
        }
    }

    StatusCard {
        visible: page.message.length > 0
        iconName: page.messageIsError ? "warning" : "check"
        warning: page.messageIsError
        title: page.messageIsError ? "Not uninstalled" : "Uninstalled"
        description: page.message

        UI.Button {
            text: "Dismiss"
            onClicked: page.message = ""
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            Repeater {
                model: apps

                UI.SettingRow {
                    id: app
                    required property string name
                    required property string appIcon
                    required property string appId
                    required property string source
                    required property string packageName
                    required property bool canUninstall
                    themeIcon: appIcon.length > 0 ? appIcon : "application-x-executable"
                    title: name
                    description: source === "store" ? "From the Store" : canUninstall ? "Installed app" : "Part of Wild OS"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: app.canUninstall
                        text: apps.busyId === app.appId ? "Uninstalling" : "Uninstall"
                        enabled: apps.busyId.length === 0
                        onClicked: {
                            confirm.appId = app.appId;
                            confirm.appName = app.name;
                            confirm.open();
                        }
                    }
                }
            }
        }
    }

    UI.Dialog {
        id: confirm

        property string appId
        property string appName

        title: "Uninstall " + appName + "?"
        text: appName + " will be removed from this computer. Your own files are not touched."
        buttons: [
            UI.Button {
                text: "Uninstall"
                kind: "primary"
                onClicked: {
                    confirm.close();
                    page.message = "";
                    apps.uninstall(confirm.appId);
                }
            },
            UI.Button {
                text: "Keep it"
                onClicked: confirm.close()
            }
        ]
    }
}
