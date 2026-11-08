import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Multitasking: snapping and Alt+Tab. Each switch writes kwinrc and asks
// KWin to reload, so the change applies without signing out.
PageScroll {
    id: page

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

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
