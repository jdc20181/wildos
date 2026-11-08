import QtQuick
import QtQuick.Layouts
import WildOS.Terminal
import WildOS.UI as UI

// Startup: which profile a new window and tab open with, and how big a new
// window is.
PageScroll {
    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "profiles"
                title: "Default profile"
                description: "Opens in a new window and in a new tab."

                Choice {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Default profile"
                    options: Config.profiles.map(p => ({ value: p.id, text: p.name }))
                    value: Config.defaultProfile
                    onChosen: v => Config.defaultProfile = v
                }
            }

            UI.SettingRow {
                iconName: "fullscreen"
                title: "When Terminal opens"
                description: "Open new windows at the size below, or filling the screen."

                Choice {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "When Terminal opens"
                    options: [
                        { value: false, text: "Normal size" },
                        { value: true, text: "Maximised" }
                    ]
                    value: Config.launchMaximised
                    onChosen: v => Config.launchMaximised = v
                }
            }

            UI.SettingRow {
                iconName: "split-right"
                title: "Launch size"
                // A greyed row says why it cannot be changed.
                description: Config.launchMaximised
                    ? "Not used while Terminal opens maximised. Choose Normal size above to set it."
                    : "How much text a new window has room for, in the font and size set under Appearance."
                enabled: !Config.launchMaximised

                NumberField {
                    anchors.verticalCenter: parent.verticalCenter
                    value: Config.launchColumns
                    from: 20
                    to: 400
                    unit: "columns"
                    accessibleName: "Columns"
                    onCommitted: v => Config.launchColumns = v
                }

                NumberField {
                    anchors.verticalCenter: parent.verticalCenter
                    value: Config.launchRows
                    from: 5
                    to: 200
                    unit: "rows"
                    accessibleName: "Rows"
                    onCommitted: v => Config.launchRows = v
                }
            }
        }
    }
}
