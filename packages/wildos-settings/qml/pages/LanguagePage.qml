import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Language and region, after Windows 11: the display language, then the
// regional format with how it shows dates, times, numbers and money. Both
// take effect when you next sign in, and the page says so plainly.
PageScroll {
    id: page

    readonly property var languages: Language.languages()
    readonly property var formats: Language.formats()
    readonly property var example: Language.examples(Language.format)

    // Typing to pick in a closed list chooses at every letter; only the
    // choice it settles on is applied, so setting up a format the computer
    // lacks asks for a password once.
    Timer {
        id: formatTimer
        property string choice
        interval: 700
        onTriggered: Language.setFormat(choice)
    }

    Connections {
        target: Language
        function onFailed(reason) {
            notice.show("The language was not changed", reason, true);
        }
    }

    Notice {
        id: notice
    }

    StatusCard {
        visible: Language.needsSignOut
        iconName: "sign-out"
        title: "Sign out to finish"
        description: "Apps switch to the new language and formats when you sign out and back in. Save your work first."

        UI.Button {
            text: "Sign out now"
            onClicked: Language.signOut()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Language"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "language"
                    title: "Display language"
                    description: "Menus, dialogs and the apps that offer it. Wild OS's own apps are in English for now."

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Display language"
                        model: page.languages
                        readonly property string choice: Language.language
                        currentIndex: model.findIndex(o => o.value === choice)
                        displayText: currentIndex >= 0 ? currentText : choice
                        onActivated: index => Language.setLanguage(model[index].value)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Region"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "globe"
                    title: "Regional format"
                    description: Language.busy ? "Getting the format ready on this computer" : "How dates, times, numbers and money are written"

                    UI.Spinner {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: Language.busy
                    }

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Regional format"
                        enabled: !Language.busy
                        model: page.formats
                        readonly property string choice: Language.format
                        currentIndex: model.findIndex(o => o.value === choice)
                        displayText: currentIndex >= 0 ? currentText : choice
                        onActivated: index => {
                            formatTimer.choice = model[index].value;
                            formatTimer.restart();
                        }
                    }
                }

                Repeater {
                    model: [
                        ["Short date", page.example.shortDate],
                        ["Long date", page.example.longDate],
                        ["Time", page.example.time],
                        ["Numbers", page.example.number],
                        ["Money", page.example.currency],
                        ["First day of the week", page.example.firstDay],
                        ["Measurements", page.example.measurement]
                    ]

                    UI.SettingRow {
                        required property var modelData
                        minHeight: 40
                        title: modelData[0]

                        UI.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData[1]
                            color: UI.Style.inkMuted
                        }
                    }
                }
            }
        }

        UI.Label {
            Layout.fillWidth: true
            visible: Language.detailed
            text: "Some formats were chosen separately in More format options. Choosing a regional format here sets them all again."
            size: 12
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Related settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    visible: Pages.exists("formatsadvanced")
                    iconName: "cog"
                    title: "More format options"
                    description: "Choose numbers, time, money and measurements separately"
                    chevron: true
                    onClicked: Navigator.open("formatsadvanced")
                }

                UI.SettingRow {
                    iconName: "clock"
                    title: "Date and time"
                    description: "Time zone and setting the clock"
                    chevron: true
                    onClicked: Navigator.open("datetime")
                }

                UI.SettingRow {
                    iconName: "keyboard"
                    title: "Keyboard"
                    description: "Layouts for the languages you type in"
                    chevron: true
                    onClicked: Navigator.open("keyboard")
                }
            }
        }
    }
}
