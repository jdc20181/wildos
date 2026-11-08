import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Time and language: the current time, date and zone at a glance, then the
// modules that change them.
PageScroll {
    id: page

    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    UI.Card {
        Layout.fillWidth: true
        implicitHeight: 112

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 24

            UI.Label {
                text: Format.time(page.now)
                size: 40
                font.weight: Font.Light
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: Format.longDate(page.now)
                    size: 15
                    font.weight: Font.DemiBold
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: "Time zone: " + System.timeZone
                    color: UI.Style.inkMuted
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: "Language: " + System.region
                    color: UI.Style.inkMuted
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                visible: Pages.exists("datetime")
                iconName: "clock"
                title: "Date and time"
                description: "Change the date, time and time zone"
                chevron: true
                onClicked: Navigator.open("datetime")
            }

            UI.SettingRow {
                visible: Pages.exists("language")
                iconName: "language"
                title: "Language and region"
                description: "Display language, formats and region"
                chevron: true
                onClicked: Navigator.open("language")
            }

            UI.SettingRow {
                visible: Pages.exists("keyboard")
                iconName: "keyboard"
                title: "Keyboard layouts"
                description: "Add a layout and switch between them"
                chevron: true
                onClicked: Navigator.open("keyboard")
            }
        }
    }
}
