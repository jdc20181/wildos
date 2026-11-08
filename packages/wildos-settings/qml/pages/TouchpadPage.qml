import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Touchpad, after Windows 11: on or off, speed, taps and scrolling. KWin
// applies each change straight away and keeps it for the touchpad.
PageScroll {
    id: page

    readonly property var pad: InputDevices.touchpads.length > 0 ? InputDevices.touchpads[0] : null

    function has(key) {
        return pad !== null && pad[key] === true;
    }

    StatusCard {
        visible: page.pad === null
        iconName: "touchpad"
        warning: true
        title: "No touchpad found"
        description: "This computer has no touchpad, or it is switched off in the computer's own settings."
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.pad !== null

        Column {
            width: parent.width

            UI.SettingRow {
                visible: page.has("supportsDisableEvents")
                iconName: "touchpad"
                title: "Touchpad"
                status: (page.pad !== null && page.pad.enabled !== false) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.pad !== null && page.pad.enabled !== false
                    Accessible.name: "Touchpad"
                    onToggled: InputDevices.set("touchpad", "enabled", checked)
                }
            }

            UI.SettingRow {
                visible: page.has("supportsDisableEventsOnExternalMouse")
                title: "Leave touchpad on when a mouse is connected"
                status: (page.pad !== null && page.pad.disableEventsOnExternalMouse !== true) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.pad !== null && page.pad.disableEventsOnExternalMouse !== true
                    Accessible.name: "Leave touchpad on when a mouse is connected"
                    onToggled: InputDevices.set("touchpad", "disableEventsOnExternalMouse", !checked)
                }
            }

            UI.SettingRow {
                id: speedRow
                visible: page.has("supportsPointerAcceleration")
                title: "Cursor speed"
                readonly property real acceleration: page.pad ? (page.pad.pointerAcceleration || 0) : 0

                UI.Slider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 240
                    accessibleName: "Cursor speed"
                    from: -1
                    to: 1
                    stepSize: 0.1
                    value: speedRow.acceleration
                    onMoved: InputDevices.set("touchpad", "pointerAcceleration", Math.round(value * 10) / 10)
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.pad !== null
        spacing: 10

        SectionTitle {
            text: "Taps and scrolling"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    visible: page.pad !== null && page.pad.tapFingerCount > 0
                    title: "Tap with a single finger to single-click"
                    status: (page.pad !== null && page.pad.tapToClick === true) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.pad !== null && page.pad.tapToClick === true
                        Accessible.name: "Tap with a single finger to single-click"
                        onToggled: InputDevices.set("touchpad", "tapToClick", checked)
                    }
                }

                UI.SettingRow {
                    visible: page.has("supportsDisableWhileTyping")
                    title: "Ignore the touchpad while typing"
                    status: (page.pad !== null && page.pad.disableWhileTyping === true) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.pad !== null && page.pad.disableWhileTyping === true
                        Accessible.name: "Ignore the touchpad while typing"
                        onToggled: InputDevices.set("touchpad", "disableWhileTyping", checked)
                    }
                }

                UI.SettingRow {
                    visible: page.has("supportsNaturalScroll")
                    title: "Scrolling follows your fingers"
                    description: "Drag two fingers up to scroll down, as on a phone"
                    status: (page.pad !== null && page.pad.naturalScroll === true) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.pad !== null && page.pad.naturalScroll === true
                        Accessible.name: "Scrolling follows your fingers"
                        onToggled: InputDevices.set("touchpad", "naturalScroll", checked)
                    }
                }
            }
        }
    }
}
