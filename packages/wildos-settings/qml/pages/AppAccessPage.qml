import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// One kind of access (camera, microphone, location, files, internet) and
// the apps from the Store that have it, after Windows 11's camera and
// microphone pages: a switch per app. Files lists each folder an app asked
// for. Changes reach an app the next time it opens.
PageScroll {
    id: page

    // Read once: the window's current page moves on before this one goes.
    property string kind: "camera"

    readonly property var copy: ({
        "camera": { icon: "camera", intro: "Apps that asked to use your camera. Turn one off and it can't see through the camera, even while open.", empty: "No apps have asked to use your camera." },
        "microphone": { icon: "mic", intro: "Apps that asked to play sound and hear your microphone. They share one switch, as the sound system gives both together.", empty: "No apps have asked to use your microphone." },
        "location": { icon: "location", intro: "Apps that asked where this computer is.", empty: "No apps have asked for your location." },
        "files": { icon: "folder", intro: "Apps that can open your folders without asking. Turned off, an app can still open a file you choose for it.", empty: "No apps can open your folders without asking." },
        "network": { icon: "globe", intro: "Apps that can go online. An app without the internet still works with your own files.", empty: "No apps from the Store yet." }
    })[kind]

    property var rows: []

    Connections {
        target: AppAccess
        function onChanged() {
            page.rows = AppAccess.apps(page.kind);
        }
    }

    Component.onCompleted: {
        kind = ({ "camera": "camera", "microphone": "microphone", "location": "location", "appfiles": "files", "appnetwork": "network" })[
            Navigator.window ? Navigator.window.currentId : ""] || "camera";
        AppAccess.reload();
    }

    UI.Label {
        Layout.fillWidth: true
        text: page.copy.intro + " Changes apply the next time the app opens."
        color: UI.Style.inkMuted
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    StatusCard {
        visible: page.kind === "location" && !System.locationServices
        iconName: "location"
        title: "Location is off"
        description: "This computer has no location service, so no app can find where it is."
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.kind !== "files"

        Column {
            width: parent.width

            Repeater {
                model: page.kind !== "files" ? page.rows : []

                UI.SettingRow {
                    id: appRow
                    required property var modelData
                    themeIcon: modelData.icon
                    title: modelData.name
                    status: modelData.allowed ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: appRow.modelData.allowed
                        // Kept as it is for when a location service arrives,
                        // but nothing to change until then, as on Windows
                        // with location services off.
                        locked: page.kind === "location" && !System.locationServices
                        lockedReason: "This computer has no location service"
                        Accessible.name: appRow.modelData.name
                        onToggled: AppAccess.set(appRow.modelData.id, appRow.modelData.token, checked)
                    }
                }
            }

            UI.SettingRow {
                visible: page.rows.length === 0
                iconName: page.copy.icon
                title: page.copy.empty
            }
        }
    }

    Repeater {
        model: page.kind === "files" ? page.rows : []

        UI.Card {
            id: appCard
            required property var modelData
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    themeIcon: appCard.modelData.icon
                    title: appCard.modelData.name
                }

                Repeater {
                    model: appCard.modelData.items

                    UI.SettingRow {
                        id: folderRow
                        required property var modelData
                        iconName: "folder"
                        title: modelData.label
                        status: modelData.allowed ? "On" : "Off"

                        UI.Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: folderRow.modelData.allowed
                            Accessible.name: appCard.modelData.name + ", " + folderRow.modelData.label
                            onToggled: AppAccess.set(appCard.modelData.id, folderRow.modelData.token, checked)
                        }
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.kind === "files" && page.rows.length === 0

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "folder"
                title: page.copy.empty
            }
        }
    }
}
