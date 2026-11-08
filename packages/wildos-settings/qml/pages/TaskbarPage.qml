import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Taskbar behaviour. The panel is changed through Plasma's own scripting
// interface, so the setting takes effect at once and survives restarts.
PageScroll {
    id: page

    readonly property var choices: [
        { mode: 0, title: "Always show the taskbar", description: "Windows stop above it" },
        { mode: 1, title: "Hide the taskbar automatically", description: "Move the pointer to the bottom of the screen to show it" },
        { mode: 2, title: "Hide it when a window covers it", description: "It stays visible until a window needs the space" }
    ]

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Taskbar behaviours"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: page.choices

                    UI.SettingRow {
                        id: choice
                        required property var modelData
                        readonly property bool selected: System.taskbarHiding === modelData.mode
                        title: modelData.title
                        description: modelData.description
                        interactive: true
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: selected
                        onClicked: System.setTaskbarHiding(modelData.mode)

                        leading: UI.RadioButton {
                            checked: choice.selected
                            focusPolicy: Qt.NoFocus
                            Accessible.ignored: true
                            onClicked: System.setTaskbarHiding(choice.modelData.mode)
                        }
                    }
                }
            }
        }
    }
}
