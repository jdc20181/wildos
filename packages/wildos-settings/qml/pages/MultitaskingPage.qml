import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Multitasking: snapping, hot corners, Alt+Tab and the taskbar across
// desktops. The snapping and Alt+Tab settings write kwinrc and ask KWin to
// reload, so the change applies without signing out; hot corners are
// wildosrc settings KWin's wildos_hotcorners plugin follows as they change;
// the taskbar choice is the taskbar's own setting, changed through
// plasmashell.
PageScroll {
    id: page

    Component.onCompleted: System.readTaskbarDesktops()

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                title: "Double-clicking a title bar will"

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Double-clicking a title bar will"
                    model: [
                        { value: "Maximize", text: "Maximise or restore" },
                        { value: "Minimize", text: "Minimise" },
                        { value: "Nothing", text: "Do nothing" }
                    ]
                    property string choice: System.config("kwinrc", "MouseBindings", "CommandTitlebarDblClick", "Maximize")
                    currentIndex: Math.max(0, model.findIndex(o => o.value === choice))
                    onActivated: index => {
                        System.setConfig("kwinrc", "MouseBindings", "CommandTitlebarDblClick", model[index].value);
                        System.reloadKWin();
                        choice = model[index].value;
                    }
                }
            }

            UI.SettingRow {
                iconName: "multitasking"
                title: "Snap windows"
                description: "Drag a window to the side of the screen to fill half of it, or to the top to maximise it"
                status: (System.snapEdges) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: System.snapEdges
                    Accessible.name: "Snap windows"
                    onToggled: System.snapEdges = checked
                }
            }

            UI.SettingRow {
                iconName: "apps"
                title: "Show snap layouts"
                description: "Hover over a window's maximise button, or press Windows key + Z, to pick a layout"
                status: (System.snapLayouts) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: System.snapLayouts
                    Accessible.name: "Show snap layouts"
                    onToggled: System.snapLayouts = checked
                }
            }

            UI.SettingRow {
                iconName: "split-right"
                title: "Suggest windows to fill the rest"
                description: "After you snap a window, pick other open windows to fill the empty spaces beside it"
                status: (System.snapAssist) ? "On" : "Off"

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: System.snapAssist
                    Accessible.name: "Suggest windows to fill the rest"
                    onToggled: System.snapAssist = checked
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            SectionTitle {
                Layout.fillWidth: true
                text: "Hot corners"
            }

            UI.LinkButton {
                text: "Learn more"
                onClicked: Qt.openUrlExternally("https://help.wildos.io/using/hot-corners/")
            }
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                // The Card has no padding of its own, as its rows reach its
                // edges, so the preview keeps its own distance.
                Item {
                    width: 1
                    height: 16
                }

                HotCornersPreview {
                    x: 16
                    width: parent.width - 32
                    settings: System.hotCorners
                }

                Item {
                    width: 1
                    height: 16
                }

                UI.SettingRow {
                    title: "Don't trigger in full-screen apps and games"
                    description: "Corners do nothing while the app in front fills the screen"
                    status: (System.hotCorners.SkipFullScreen) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: System.hotCorners.SkipFullScreen
                        Accessible.name: "Don't trigger in full-screen apps and games"
                        onToggled: System.setHotCorner("SkipFullScreen", checked)
                    }
                }

                UI.SettingRow {
                    title: "Delay"
                    description: "How long the pointer rests in a corner before it acts"

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Repeater {
                            model: [
                                { value: 0, text: "Instant" },
                                { value: 150, text: "Short" },
                                { value: 400, text: "Long" }
                            ]

                            UI.SegmentButton {
                                required property var modelData
                                text: modelData.text
                                checked: System.hotCorners.Delay === modelData.value
                                onClicked: System.setHotCorner("Delay", modelData.value)
                            }
                        }
                    }
                }

                UI.SettingRow {
                    title: "Reset to defaults"
                    description: "Task view top left, notifications top right, nothing at the bottom"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Reset"
                        Accessible.name: "Reset hot corners to defaults"
                        onClicked: System.resetHotCorners()
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Desktops"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "On the taskbar, show all open windows"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "On the taskbar, show all open windows"
                        model: [
                            { value: false, text: "Only on the desktop I'm using" },
                            { value: true, text: "On all desktops" }
                        ]
                        readonly property bool choice: System.taskbarAllDesktops
                        currentIndex: model.findIndex(o => o.value === choice)
                        onActivated: index => System.setTaskbarAllDesktops(model[index].value)
                    }
                }

                UI.SettingRow {
                    title: "Show windows from all desktops in Alt+Tab"
                    description: "When this is off, Alt+Tab only shows windows on the desktop you are using"
                    status: (System.altTabAllDesktops) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: System.altTabAllDesktops
                        Accessible.name: "Show windows from all desktops in Alt+Tab"
                        onToggled: System.altTabAllDesktops = checked
                    }
                }

                UI.SettingRow {
                    visible: Pages.exists("desktops")
                    iconName: "desktops"
                    title: "Desktops"
                    description: "Extra desktops for separate sets of windows"
                    chevron: true
                    onClicked: Navigator.open("desktops")
                }
            }
        }
    }

    // Task view, the desktop and switching desktops are also touchpad
    // swipes, set on their own page, which only exists with a touchpad.
    ColumnLayout {
        Layout.fillWidth: true
        visible: InputDevices.touchpads.length > 0
        spacing: 10

        SectionTitle {
            text: "Related settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "touchpad"
                    title: "Looking for gestures?"
                    description: "Swipe on the touchpad for Task view, the desktop and other desktops"
                    chevron: true
                    onClicked: Navigator.open("touchpadgestures")
                }
            }
        }
    }
}
