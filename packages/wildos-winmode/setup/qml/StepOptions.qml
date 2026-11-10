import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import WildOS.WinMode.Backend

// 9.4, Choose options (WinModeOptions.dc). The choices are the ones
// Settings > Windows apps > Windows mode keeps, so they can all change
// later.
Page {
    id: page

    signal back()
    signal setUp()

    // Windows' own licence page is skipped by the answer file, so this is
    // where the terms are accepted, and nothing starts until they are.
    property bool termsAccepted: false

    title: "Choose options"
    text: "These suit most people. You can change any of them later in Settings, under Windows apps."
    note: Setup.office ? "About 30 GB in total, including Microsoft 365." : "About 26 GB in total."

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.ListRow {
                title: "Install Microsoft 365 apps"
                description: "Word, Excel, PowerPoint, Outlook and OneNote. Sign in with your Microsoft account once it's ready."
                interactive: true
                onClicked: Setup.office = !Setup.office
                leading: UI.CheckBox {
                    checked: Setup.office
                    Accessible.name: "Install Microsoft 365 apps"
                    onToggled: Setup.office = checked
                }

                UI.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "4 GB"
                    size: 12
                    color: UI.Style.inkMuted
                }
            }

            UI.ListRow {
                visible: Setup.office
                title: "Your Microsoft 365 plan"
                description: "Decides which edition of the apps to install"
                leading: Item {
                    width: 20
                    height: 1
                }

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Microsoft 365 plan"
                    model: [
                        { value: "personal", text: "Personal or Family" },
                        { value: "business", text: "Business" },
                        { value: "work", text: "Work or school" }
                    ]
                    currentIndex: model.findIndex(o => o.value === Setup.officePlan)
                    onActivated: Setup.officePlan = currentValue
                }
            }

            UI.ListRow {
                title: "Open Windows files in Windows apps"
                description: "Double-clicking a .docx, .xlsx or .pptx opens it in Word, Excel or PowerPoint"
                interactive: true
                onClicked: Setup.openFiles = !Setup.openFiles
                leading: UI.CheckBox {
                    checked: Setup.openFiles
                    Accessible.name: "Open Windows files in Windows apps"
                    onToggled: Setup.openFiles = checked
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        UI.Label {
            text: "Speed and memory"
            size: 15
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.ListRow {
                    title: "When no Windows app is open"
                    description: "Automatic keeps it ready in memory when plugged in, and saves it to disk on battery or when memory runs low"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "When no Windows app is open"
                        model: [
                            { value: "automatic", text: "Automatic" },
                            { value: "memory", text: "Keep ready in memory" },
                            { value: "disk", text: "Save to disk" },
                            { value: "shutdown", text: "Shut down Windows" }
                        ]
                        currentIndex: model.findIndex(o => o.value === Setup.whenIdle)
                        onActivated: Setup.whenIdle = currentValue
                    }
                }

                UI.ListRow {
                    title: "Memory Windows mode can use"
                    description: "More helps big spreadsheets and Photoshop. Unused memory is handed back."

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Memory limit"
                        model: Setup.memoryChoices
                        currentIndex: model.findIndex(o => o.value === Setup.memoryGb)
                        onActivated: Setup.memoryGb = currentValue
                    }
                }

                UI.ListRow {
                    visible: Setup.hyperThreadingAvailable
                    title: "Use every processor thread"
                    description: "Faster heavy apps, one less CPU-flaw protection for the whole PC. Needs a restart."
                    interactive: true
                    onClicked: Setup.hyperThreading = !Setup.hyperThreading

                    leading: UI.CheckBox {
                        checked: Setup.hyperThreading
                        Accessible.name: "Use every processor thread"
                        onToggled: Setup.hyperThreading = checked
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        UI.Label {
            text: "Your files"
            size: 15
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.ListRow {
                    title: "Folders Windows apps can see"
                    description: "Saving in Word or Excel goes straight into these folders on Wild OS"

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Repeater {
                            model: Setup.folders

                            UI.Chip {
                                required property string modelData
                                text: modelData
                            }
                        }
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.ListRow {
                title: "I accept Microsoft's licence terms"
                description: Setup.office
                    ? "Windows and Microsoft 365 are installed under Microsoft's licence terms."
                    : "Windows is installed under Microsoft's licence terms."
                interactive: true
                onClicked: page.termsAccepted = !page.termsAccepted
                leading: UI.CheckBox {
                    checked: page.termsAccepted
                    Accessible.name: "I accept Microsoft's licence terms"
                    onToggled: page.termsAccepted = checked
                }
            }
        }
    }

    buttons: [
        UI.LinkButton {
            text: "Read the terms"
            size: 12
            onClicked: Setup.openUrl("https://www.microsoft.com/en-gb/useterms")
        },
        UI.Button {
            text: "Back"
            onClicked: page.back()
        },
        UI.Button {
            kind: "primary"
            text: "Set up"
            enabled: Setup.spaceOk && page.termsAccepted
            onClicked: page.setUp()
        }
    ]
}
