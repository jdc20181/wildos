import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Privacy and security, laid out as Settings.dc.html: the protection status
// from security-status, a tile per protection, the protection level, what
// apps can reach, searching files and activity history, then USB protection
// and usage data.
PageScroll {
    id: page

    readonly property var perms: AppAccess.summary
    property string usbError
    property string levelError

    function appCount(n, what) {
        if (n === 0) {
            return "No apps can use your " + what;
        }
        return Format.plural(n, "app", "apps") + " can use your " + what;
    }

    // Keeps "Last checked 10 minutes ago" true while the page is open.
    property date now: new Date()
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    Connections {
        target: System
        function onUsbProtectionFailed(reason) {
            page.usbError = reason;
            usbToggle.checked = Qt.binding(() => Protection.status.devices === "on");
        }
        function onSecurityChanged() {
            page.usbError = "";
        }
        function onProtectionLevelFailed(reason) {
            page.levelError = reason;
        }
    }

    StatusCard {
        descriptionLineHeight: 19.5
        iconName: Protection.busy ? "update" : Protection.allOn ? "shield-check" : "shield-alert"
        spinning: Protection.busy
        warning: !Protection.busy && Protection.checked && !Protection.allOn
        title: Protection.busy ? "Checking your protection" : Protection.headline
        description: {
            page.now;
            if (Protection.busy) {
                return Protection.checkStep + ".";
            }
            const note = Protection.checkNote.length > 0 ? " " + Protection.checkNote : "";
            return Protection.description(System.securityChecked) + note;
        }

        UI.Button {
            visible: Protection.updatesWaiting && !Protection.busy
            text: "Install updates"
            kind: "primary"
            onClicked: Navigator.open("update")
        }

        UI.Button {
            text: Protection.busy ? "Checking" : "Check now"
            busy: Protection.busy
            onClicked: Protection.checkNow()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        visible: Protection.tiles.length > 0
        columns: 3
        columnSpacing: 10
        rowSpacing: 10

        Repeater {
            model: Protection.tiles

            Tile {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                iconName: modelData.icon
                title: modelData.title
                value: Protection.value(modelData)
                warning: !Protection.good(modelData)
                interactive: true
                onClicked: protectionDialog.show(modelData)
            }
        }
    }

    // Standard is what Wild OS ships. Strict adds a firewall and holds back
    // unknown USB devices found at start-up; Maximum also blocks network
    // discovery, Bluetooth and the camera (settings-protection). Both
    // shorten the screen lock.
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Protection level"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            UI.ChoiceCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                showIndicator: true
                checkable: false
                title: "Standard"
                description: "Strong protection that stays out of your way. Recommended for most people."
                checked: System.protectionLevel === "standard"
                enabled: !System.protectionBusy
                onClicked: {
                    page.levelError = "";
                    System.setProtectionLevel("standard");
                }
            }

            UI.ChoiceCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                showIndicator: true
                checkable: false
                title: "Strict"
                description: "Tighter defaults. Some apps and devices will ask for permission first."
                checked: System.protectionLevel === "strict"
                enabled: !System.protectionBusy
                onClicked: {
                    page.levelError = "";
                    System.setProtectionLevel("strict");
                }
            }

            UI.ChoiceCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                showIndicator: true
                checkable: false
                title: "Maximum"
                description: "For high-risk situations. Expect some things to stop working."
                checked: System.protectionLevel === "maximum"
                enabled: !System.protectionBusy
                onClicked: {
                    page.levelError = "";
                    System.setProtectionLevel("maximum");
                }
            }
        }

        UI.Label {
            Layout.fillWidth: true
            visible: page.levelError.length > 0
            text: page.levelError
            size: 12
            color: UI.Style.danger
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "App permissions"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "camera"
                    title: "Camera"
                    description: page.appCount(page.perms.camera || 0, "camera")
                    chevron: true
                    onClicked: Navigator.open("camera")
                }

                UI.SettingRow {
                    iconName: "mic"
                    title: "Microphone"
                    description: page.appCount(page.perms.microphone || 0, "microphone")
                    chevron: true
                    onClicked: Navigator.open("microphone")
                }

                UI.SettingRow {
                    iconName: "location"
                    title: "Location"
                    description: !System.locationServices ? "Off for all apps"
                        : (page.perms.location || 0) > 0 ? Format.plural(page.perms.location, "app", "apps") + " can use your location"
                        : "Apps ask before using your location"
                    chevron: true
                    onClicked: Navigator.open("location")
                }

                UI.SettingRow {
                    iconName: "folder"
                    title: "Files and folders"
                    description: (page.perms.files || 0) === 0
                        ? "Apps ask before opening anything outside their own folder"
                        : Format.plural(page.perms.files, "app", "apps") + " can open your files without asking"
                    chevron: true
                    onClicked: Navigator.open("appfiles")
                }

                UI.SettingRow {
                    iconName: "sandbox"
                    title: "All app permissions"
                    description: "Internet and everything else apps from the Store can reach"
                    chevron: true
                    onClicked: Navigator.open("apppermissions")
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Search and history"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "search"
                    title: "Searching files"
                    description: !FileSearch.available ? "File indexing isn't installed"
                        : FileSearch.enabled ? "Your files are indexed so search finds them at once" : "Indexing is off"
                    chevron: true
                    onClicked: Navigator.open("searching")
                }

                UI.SettingRow {
                    iconName: "clock"
                    title: "Activity history"
                    description: ActivityHistory.remember ? "Recent files are remembered on this computer" : "Recent files are not remembered"
                    chevron: true
                    onClicked: Navigator.open("activityhistory")
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                minHeight: 64
                title: "Share usage data with Wild OS"
                description: "Wild OS does not collect any. Nothing about how you use this computer is sent anywhere."
                status: "Off"

                // There is nothing to send, so the switch stays off and says why.
                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: false
                    locked: true
                    lockedReason: "Wild OS has no usage data to share"
                    Accessible.name: "Share usage data with Wild OS"
                }
            }

            UI.SettingRow {
                visible: Protection.status.devices !== undefined
                minHeight: 64
                title: "Block new USB devices"
                description: page.usbError.length > 0 ? page.usbError
                    : "Stops anything plugged in from working until you allow it from the shield on the taskbar."
                status: (Protection.status.devices === "on") ? "On" : "Off"

                UI.Toggle {
                    id: usbToggle
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Protection.status.devices === "on"
                    enabled: !(System.securityChecking)
                    Accessible.name: "Block new USB devices"
                    onToggled: System.setUsbProtection(checked)
                }
            }
        }
    }

    ProtectionDialog {
        id: protectionDialog
    }
}
