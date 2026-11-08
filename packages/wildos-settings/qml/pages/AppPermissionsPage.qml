import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// App permissions, after Windows 11's Privacy and security list: one row
// for each thing an app can reach, saying how many apps can, each opening
// the list of apps with their switches.
PageScroll {
    id: page

    readonly property var summary: AppAccess.summary

    function line(kind, noun) {
        const allowed = summary[kind] || 0;
        const asked = summary[kind + "Asked"] || 0;
        if (asked === 0) {
            return "No apps have asked to use your " + noun;
        }
        if (allowed === 0) {
            return "Turned off for every app that asked";
        }
        return Format.plural(allowed, "app", "apps") + " can use your " + noun;
    }

    Component.onCompleted: AppAccess.reload()

    UI.Label {
        Layout.fillWidth: true
        text: "Apps from the Store run in a sandbox and can only reach what they asked for when you installed them. Turn any of it off here. They can always open a file you choose for them."
        color: UI.Style.inkMuted
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "camera"
                title: "Camera"
                description: page.line("camera", "camera")
                chevron: true
                onClicked: Navigator.open("camera")
            }

            UI.SettingRow {
                iconName: "mic"
                title: "Microphone"
                description: page.line("microphone", "microphone")
                chevron: true
                onClicked: Navigator.open("microphone")
            }

            UI.SettingRow {
                iconName: "location"
                title: "Location"
                description: !System.locationServices ? "Off for all apps, as this computer has no location service"
                    : page.line("location", "location")
                chevron: true
                onClicked: Navigator.open("location")
            }

            UI.SettingRow {
                iconName: "folder"
                title: "Files and folders"
                description: (page.summary.files || 0) === 0 ? "Apps ask before opening anything outside their own folder"
                    : Format.plural(page.summary.files, "app", "apps") + " can open your files without asking"
                chevron: true
                onClicked: Navigator.open("appfiles")
            }

            UI.SettingRow {
                iconName: "globe"
                title: "Internet"
                description: AppAccess.appCount === 0 ? "No apps from the Store yet"
                    : Format.plural(page.summary.network || 0, "app", "apps") + " can go online"
                chevron: true
                onClicked: Navigator.open("appnetwork")
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: Pages.exists("apppermissionsadvanced")
        spacing: 10

        SectionTitle {
            text: "Related settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "sandbox"
                    title: "Everything an app can reach"
                    description: "Every sandbox permission, app by app, for experts"
                    chevron: true
                    onClicked: Navigator.open("apppermissionsadvanced")
                }
            }
        }
    }
}
