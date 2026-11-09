import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Typing, after Windows 11's Time and language > Typing: spell checking,
// the languages it checks in and the words you have taught it. It covers
// the apps that check spelling through KDE's Sonnet; browsers and office
// suites keep their own spelling settings.
PageScroll {
    id: page

    readonly property bool hasDictionaries: Spelling.dictionaries.length > 0
    readonly property var others: Spelling.dictionaries.filter(d => d.value !== Spelling.language)

    function addWord() {
        if (Spelling.addWord(wordField.text)) {
            wordField.clear();
        }
    }

    StatusCard {
        visible: !page.hasDictionaries
        iconName: "warning"
        warning: true
        title: "No spelling dictionaries are installed"
        description: "Spell checking needs a dictionary for each language. Install one, such as hunspell-en-gb, and open this page again."
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Spelling"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "pencil"
                    title: "Check spelling"
                    description: "Underline misspelt words in apps that check spelling"
                    status: Spelling.enabled ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Spelling.enabled
                        Accessible.name: "Check spelling"
                        onToggled: Spelling.setEnabled(checked)
                    }
                }

                UI.SettingRow {
                    title: "Check as I type"
                    description: "Look over each word as you write it, not only when you ask"
                    enabled: Spelling.enabled
                    status: Spelling.asYouType ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Spelling.asYouType
                        Accessible.name: "Check as I type"
                        onToggled: Spelling.setAsYouType(checked)
                    }
                }

                UI.SettingRow {
                    title: "Ignore words in capitals"
                    description: "Leave words such as NASA or HTML alone"
                    enabled: Spelling.enabled
                    status: Spelling.skipUppercase ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Spelling.skipUppercase
                        Accessible.name: "Ignore words in capitals"
                        onToggled: Spelling.setSkipUppercase(checked)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        visible: page.hasDictionaries

        SectionTitle {
            text: "Languages"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "language"
                    title: "Spelling language"
                    description: "The dictionary used first"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Spelling language"
                        model: Spelling.dictionaries
                        readonly property string choice: Spelling.language
                        currentIndex: model.findIndex(o => o.value === choice)
                        displayText: currentIndex >= 0 ? currentText : choice
                        onActivated: index => Spelling.setLanguage(model[index].value)
                    }
                }

                UI.SettingRow {
                    visible: page.others.length > 0
                    title: "Detect the language I'm writing in"
                    description: "Switch to whichever of your languages a sentence is in"
                    status: Spelling.autodetect ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Spelling.autodetect
                        Accessible.name: "Detect the language I'm writing in"
                        onToggled: Spelling.setAutodetect(checked)
                    }
                }

                Repeater {
                    model: Spelling.autodetect ? page.others : []

                    UI.SettingRow {
                        id: other
                        required property var modelData
                        minHeight: 44
                        title: modelData.text

                        leading: UI.CheckBox {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: Spelling.otherLanguages.includes(other.modelData.value)
                            Accessible.name: "Also check " + other.modelData.text
                            onToggled: Spelling.setOtherLanguage(other.modelData.value, checked)
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        visible: page.hasDictionaries

        SectionTitle {
            text: "Your dictionary"
        }

        UI.Label {
            Layout.fillWidth: true
            text: "Words here are never marked as misspelt. Names, places and words from your work are good ones to add."
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "Add a word"

                    UI.TextField {
                        id: wordField
                        anchors.verticalCenter: parent.verticalCenter
                        width: 220
                        placeholderText: "Type a word"
                        onAccepted: page.addWord()
                    }

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Add"
                        iconName: "plus"
                        enabled: wordField.text.trim().length > 0 && !wordField.text.trim().includes(" ")
                        onClicked: page.addWord()
                    }
                }

                UI.SettingRow {
                    visible: Spelling.words.length === 0
                    title: "No words added yet"
                    description: "Add words here, or choose Add to dictionary when an app underlines one"
                }

                Repeater {
                    model: Spelling.words

                    UI.SettingRow {
                        id: wordRow
                        required property string modelData
                        minHeight: 44
                        title: modelData

                        UI.IconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "delete"
                            tip: "Remove " + wordRow.modelData
                            onClicked: Spelling.removeWord(wordRow.modelData)
                        }
                    }
                }
            }
        }
    }
}
