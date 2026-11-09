import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Thunderbolt: the docks, drives and screens plugged into a Thunderbolt or
// USB4 port. A new device stays shut out until someone allows it, as a
// device on these ports can reach the PC's memory; once allowed it is
// remembered until forgotten.
PageScroll {
    id: page

    readonly property bool usable: Thunderbolt.available && Thunderbolt.hasPorts && !Thunderbolt.firmwareOff

    Component.onCompleted: Thunderbolt.reload()

    function statusText(device) {
        switch (device.status) {
        case "authorized": return device.stored ? "Connected, allowed" : "Connected, allowed until it is unplugged";
        case "connected": return "Waiting to be allowed";
        case "authorizing": return "Allowing";
        case "auth-error": return "Could not be allowed";
        case "connecting": return "Connecting";
        case "disconnected": return "Not connected, allowed when plugged in";
        default: return "Not ready";
        }
    }

    Connections {
        target: Thunderbolt
        function onFailed(title, reason) {
            notice.show(title, reason, true);
        }
    }

    Notice {
        id: notice
    }

    StatusCard {
        visible: !page.usable
        iconName: "bolt"
        warning: true
        title: Thunderbolt.firmwareOff ? "Thunderbolt is turned off in this PC's firmware" : "This PC has no Thunderbolt ports"
        description: Thunderbolt.firmwareOff ? "Turn on Thunderbolt support in the PC's UEFI settings to use Thunderbolt devices."
            : ""
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.usable
        spacing: 10

        SectionTitle {
            text: "Devices"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    id: deviceList
                    model: Thunderbolt.devices

                    UI.SettingRow {
                        id: deviceRow
                        required property var modelData
                        readonly property bool busy: Thunderbolt.busy === modelData.uid || modelData.status === "authorizing"
                        iconName: "bolt"
                        title: modelData.vendor && modelData.vendor.length > 0 ? modelData.vendor + " " + modelData.name : modelData.name
                        description: page.statusText(modelData)
                        descriptionColor: modelData.status === "connected" || modelData.status === "auth-error" ? UI.Style.warning : UI.Style.inkMuted

                        UI.Button {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: deviceRow.modelData.status === "connected" || deviceRow.modelData.status === "auth-error"
                                || (deviceRow.modelData.status === "authorized" && !deviceRow.modelData.stored)
                            kind: deviceRow.modelData.status === "authorized" ? "secondary" : "primary"
                            text: deviceRow.modelData.status === "authorized" ? "Always allow" : "Allow"
                            busy: deviceRow.busy
                            enabled: Thunderbolt.busy.length === 0
                            onClicked: Thunderbolt.allow(deviceRow.modelData.uid)
                        }

                        UI.Button {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: deviceRow.modelData.stored === true
                            text: "Forget"
                            busy: deviceRow.busy
                            enabled: Thunderbolt.busy.length === 0
                            onClicked: Thunderbolt.forget(deviceRow.modelData.uid)
                        }
                    }
                }

                UI.SettingRow {
                    visible: deviceList.count === 0
                    title: "No Thunderbolt devices connected"
                    description: "Plug in a dock or other Thunderbolt device and it appears here."
                }
            }
        }
    }
}
