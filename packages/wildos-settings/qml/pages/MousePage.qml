import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Mouse, after Windows 11: primary button, pointer speed and scrolling,
// for every mouse at once. KWin applies each change straight away and
// keeps it for the device.
PageScroll {
    id: page

    readonly property var mouse: InputDevices.mice.length > 0 ? InputDevices.mice[0] : null

    function has(key) {
        return mouse !== null && mouse[key] === true;
    }

    StatusCard {
        visible: page.mouse === null
        iconName: "mouse"
        warning: true
        title: "No mouse found"
        description: "Plug in a mouse, or pair one in Bluetooth, and its settings appear here."
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.mouse !== null

        Column {
            width: parent.width

            UI.SettingRow {
                visible: page.has("supportsLeftHanded")
                iconName: "mouse"
                title: "Primary mouse button"

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Primary mouse button"
                    model: [{ value: false, text: "Left" }, { value: true, text: "Right" }]
                    readonly property var choice: page.mouse ? page.mouse.leftHanded === true : false
                    currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                    onActivated: index => InputDevices.set("mouse", "leftHanded", model[index].value)
                }
            }

            UI.SettingRow {
                id: speedRow
                visible: page.has("supportsPointerAcceleration")
                title: "Mouse pointer speed"
                readonly property real acceleration: page.mouse ? (page.mouse.pointerAcceleration || 0) : 0

                UI.Slider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 240
                    accessibleName: "Mouse pointer speed"
                    from: -1
                    to: 1
                    stepSize: 0.1
                    value: speedRow.acceleration
                    onMoved: InputDevices.set("mouse", "pointerAcceleration", Math.round(value * 10) / 10)
                }
            }

            UI.SettingRow {
                visible: page.has("supportsPointerAccelerationProfileAdaptive")
                title: "Enhance pointer precision"
                description: "The pointer moves further when you move the mouse quickly"
                status: (page.mouse !== null && page.mouse.pointerAccelerationProfileAdaptive === true) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.mouse !== null && page.mouse.pointerAccelerationProfileAdaptive === true
                    Accessible.name: "Enhance pointer precision"
                    onToggled: {
                        InputDevices.set("mouse", "pointerAccelerationProfileAdaptive", checked);
                        InputDevices.set("mouse", "pointerAccelerationProfileFlat", !checked);
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.mouse !== null
        spacing: 10

        SectionTitle {
            text: "Scrolling"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    id: scrollRow
                    title: "Lines to scroll at a time"
                    readonly property real factor: page.mouse ? (page.mouse.scrollFactor || 1) : 1
                    status: Math.round(factor * 3) + (Math.round(factor * 3) === 1 ? " line" : " lines")

                    UI.Slider {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 240
                        accessibleName: "Lines to scroll at a time"
                        from: 1
                        to: 15
                        stepSize: 1
                        value: Math.round(scrollRow.factor * 3)
                        onMoved: InputDevices.set("mouse", "scrollFactor", value / 3)
                    }
                }

                UI.SettingRow {
                    visible: page.has("supportsNaturalScroll")
                    title: "Reverse scrolling direction"
                    description: "Content moves the way your finger does, as on a phone"
                    status: (page.mouse !== null && page.mouse.naturalScroll === true) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.mouse !== null && page.mouse.naturalScroll === true
                        Accessible.name: "Reverse scrolling direction"
                        onToggled: InputDevices.set("mouse", "naturalScroll", checked)
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
                visible: Pages.exists("touchpad") && InputDevices.touchpads.length > 0
                iconName: "touchpad"
                title: "Touchpad"
                description: "Taps, scrolling and gestures"
                chevron: true
                onClicked: Navigator.open("touchpad")
            }
        }
    }
}
