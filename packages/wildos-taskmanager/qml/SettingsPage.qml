import QtQuick
import QtQuick.Layouts
import WildOS.TaskManager
import WildOS.UI as UI

// Task Manager's own settings, as Windows has them: the page it opens on,
// how often the numbers update (or paused), and whether the system's own
// processes are listed.
Item {
    id: page

    readonly property var startPages: [
        { value: "processes", text: "Processes" },
        { value: "performance", text: "Performance" },
        { value: "startup", text: "Startup apps" },
        { value: "windowsapps", text: "Windows apps" },
        { value: "services", text: "Services" }
    ]
    readonly property var speeds: [
        { value: "high", text: "High (twice a second)" },
        { value: "normal", text: "Normal (every second)" },
        { value: "low", text: "Low (every 4 seconds)" },
        { value: "paused", text: "Paused" }
    ]

    PageHeader {
        id: header
        width: parent.width
        title: "Settings"
    }

    Column {
        x: 22
        y: header.height + 20
        width: Math.min(parent.width - 44, 720)
        spacing: 4

        UI.Card {
            width: parent.width
            implicitHeight: cardColumn.implicitHeight

            Column {
                id: cardColumn
                width: parent.width

                UI.SettingRow {
                    width: parent.width
                    iconName: "processes"
                    title: "Default start page"
                    description: "The page Task Manager opens on"

                    UI.ComboBox {
                        width: 200
                        model: page.startPages
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: Math.max(0, page.startPages.findIndex(p => p.value === Prefs.startPage))
                        onActivated: Prefs.startPage = currentValue
                    }
                }

                UI.SettingRow {
                    width: parent.width
                    iconName: "refresh"
                    title: "Update speed"
                    description: "How often the numbers and graphs change"

                    UI.ComboBox {
                        width: 220
                        model: page.speeds
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: Math.max(0, page.speeds.findIndex(s => s.value === Prefs.updateSpeed))
                        onActivated: Prefs.updateSpeed = currentValue
                    }
                }

                UI.SettingRow {
                    width: parent.width
                    iconName: "system"
                    title: "Show Wild OS processes"
                    description: "List what the system runs for itself under Processes"

                    UI.Toggle {
                        checked: Prefs.showSystem
                        onToggled: Prefs.showSystem = checked
                    }
                }
            }
        }
    }
}
