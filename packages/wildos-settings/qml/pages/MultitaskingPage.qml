import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Multitasking: snapping, screen corners, Alt+Tab and the taskbar across
// desktops. The snapping, corner and Alt+Tab settings write kwinrc and ask
// KWin to reload, so the change applies without signing out; the taskbar
// choice is the taskbar's own setting, changed through plasmashell.
PageScroll {
    id: page

    Component.onCompleted: System.readTaskbarDesktops()

    readonly property var cornerActions: [
        { value: "taskview", text: "Task view" },
        { value: "desktop", text: "Show the desktop" },
        { value: "lock", text: "Lock the screen" },
        { value: "none", text: "Nothing" }
    ]

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

        SectionTitle {
            text: "Screen corners"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: [
                        { corner: 7, title: "Top left corner" },
                        { corner: 1, title: "Top right corner" },
                        { corner: 5, title: "Bottom left corner" },
                        { corner: 3, title: "Bottom right corner" }
                    ]

                    UI.SettingRow {
                        id: cornerRow
                        required property var modelData
                        title: modelData.title

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: cornerRow.modelData.title
                            model: page.cornerActions
                            readonly property string choice: System.screenCorners[String(cornerRow.modelData.corner)] ?? "none"
                            currentIndex: model.findIndex(o => o.value === choice)
                            onActivated: index => System.setScreenCorner(cornerRow.modelData.corner, model[index].value)
                        }
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
}
