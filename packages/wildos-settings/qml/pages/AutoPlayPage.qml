import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// AutoPlay, after Windows 11: one switch for every drive and memory card,
// then when it applies, and the drives this PC has seen. "Ready" here is
// what Plasma's automounter does: the drive is mounted and shows in Files
// without a click. Nothing on a drive is ever run.
PageScroll {
    id: page

    readonly property bool on: AutoPlay.onPlugin || AutoPlay.onLogin

    Component.onCompleted: AutoPlay.reload()

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "play"
                title: "Use AutoPlay for all media and devices"
                description: "Drives and memory cards are ready in Files as soon as you plug them in"
                status: page.on ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.on
                    Accessible.name: "Use AutoPlay for all media and devices"
                    onToggled: {
                        AutoPlay.setOnPlugin(checked);
                        AutoPlay.setOnLogin(checked);
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.on
        spacing: 10

        SectionTitle {
            text: "When to use AutoPlay"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "When you plug in a drive or memory card"
                    status: AutoPlay.onPlugin ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: AutoPlay.onPlugin
                        Accessible.name: "When you plug in a drive or memory card"
                        onToggled: AutoPlay.setOnPlugin(checked)
                    }
                }

                UI.SettingRow {
                    title: "When you sign in"
                    description: "For drives that are already plugged in"
                    status: AutoPlay.onLogin ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: AutoPlay.onLogin
                        Accessible.name: "When you sign in"
                        onToggled: AutoPlay.setOnLogin(checked)
                    }
                }

                UI.SettingRow {
                    title: "Include drives this PC hasn't seen before"
                    description: "Otherwise only drives you've opened here before are made ready"
                    status: AutoPlay.unknownDevices ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: AutoPlay.unknownDevices
                        Accessible.name: "Include drives this PC hasn't seen before"
                        onToggled: AutoPlay.setUnknownDevices(checked)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Drives"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    id: driveList
                    model: AutoPlay.drives

                    UI.SettingRow {
                        id: driveRow
                        required property var modelData
                        iconName: "usb"
                        title: modelData.name

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: "AutoPlay for " + driveRow.modelData.name
                            model: [{ value: false, text: "Use the settings above" }, { value: true, text: "Always" }]
                            readonly property bool choice: driveRow.modelData.always === true
                            currentIndex: model.findIndex(o => o.value === choice)
                            onActivated: index => AutoPlay.setAlways(driveRow.modelData.udi, model[index].value)
                        }

                        UI.Button {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Forget"
                            onClicked: AutoPlay.forget(driveRow.modelData.udi)
                        }
                    }
                }

                UI.SettingRow {
                    visible: driveList.count === 0
                    title: "No drives yet"
                    description: "Drives and memory cards you plug in appear here."
                }
            }
        }
    }
}
