import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// Choose options: 9.4 in the setup window and 3.10 in first run, the same
// page with its own title and main button ("Set up", or "Finish"). The
// choices are the ones Settings > Windows apps > Windows mode keeps, so
// they can all change later; they live in Setup, which builds the request
// from them.
Page {
    id: page

    property string mainText: "Set up"
    property bool busy: false

    signal back()
    signal confirmed()

    title: "Choose options"
    text: "These suit most people. You can change any of them later in Settings, under Windows apps."
    spacing: 16

    readonly property string editionName: Setup.edition === "pro" ? "Windows 11 Pro" : Setup.edition === "home" ? "Windows 11 Home" : ""
    readonly property bool keyChosen: Setup.licenceChoice === "key"

    function link(href, words) {
        return "<a href=\"" + href + "\" style=\"color: " + UI.Style.accentText + "; text-decoration: none;\">" + words + "</a>";
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            OptionRow {
                width: parent.width
                title: "Windows licence"
                description: {
                    if (Setup.firmwareUsable) {
                        return (page.editionName.length > 0 ? page.editionName + " found in this PC." : "A licence found in this PC.")
                            + " Or enter a product key, or activate later.";
                    }
                    if (Setup.edition === "home" && (Setup.licence === "firmware" || Setup.licence === "inuse")) {
                        return "This PC's Windows 11 Home licence can't activate Windows mode. Enter a Pro key, or activate later.";
                    }
                    return "No licence found in this PC. Enter a product key, or activate later.";
                }

                UI.ComboBox {
                    Accessible.name: "Windows licence"
                    model: (Setup.firmwareUsable ? [{ value: "firmware", text: "Built into this PC" }] : [])
                        .concat([{ value: "key", text: "Enter a product key" }, { value: "none", text: "Activate later" }])
                    currentIndex: model.findIndex(o => o.value === Setup.licenceChoice)
                    onActivated: Setup.licenceChoice = currentValue
                }
            }

            OptionRow {
                width: parent.width
                visible: page.keyChosen
                title: "Product key"
                description: keyField.error ? "That isn't the right shape. A key has 25 letters and numbers in five groups."
                    : "Windows 11 Pro, from a sticker or an email"
                descriptionColor: keyField.error ? UI.Style.danger : UI.Style.inkMuted
                leading: Item {}

                KeyField {
                    id: keyField
                    implicitWidth: 300
                    text: Setup.productKey
                    onTextChanged: if (Setup.productKey !== text) Setup.productKey = text
                }
            }

            OptionRow {
                width: parent.width
                interactive: true
                onClicked: Setup.office = !Setup.office
                title: "Install Microsoft Office"
                description: "Optional. Word, Excel, PowerPoint and Outlook. Needs a subscription or an Office licence."
                leading: UI.CheckBox {
                    checked: Setup.office
                    Accessible.name: "Install Microsoft Office"
                    onToggled: Setup.office = checked
                }

                UI.Label {
                    text: "4 GB"
                    size: 12
                    color: UI.Style.inkMuted
                }
            }

            OptionRow {
                width: parent.width
                visible: Setup.office
                title: "Your Office plan"
                description: "Personal or Family, Business, Work or school, or a product key"
                leading: Item {}

                UI.ComboBox {
                    Accessible.name: "Office plan"
                    model: [
                        { value: "personal", text: "Personal or Family" },
                        { value: "business", text: "Business" },
                        { value: "work", text: "Work or school" },
                        { value: "key", text: "Product key" }
                    ]
                    currentIndex: model.findIndex(o => o.value === Setup.officePlan)
                    onActivated: Setup.officePlan = currentValue
                }
            }

            OptionRow {
                width: parent.width
                interactive: true
                onClicked: Setup.openFiles = !Setup.openFiles
                title: "Open Windows files in Windows apps"
                description: "Double-clicking a .docx, .xlsx or .pptx opens it in Word, Excel or PowerPoint"
                leading: UI.CheckBox {
                    checked: Setup.openFiles
                    Accessible.name: "Open Windows files in Windows apps"
                    onToggled: Setup.openFiles = checked
                }
            }
        }
    }

    component Heading: UI.Label {
        size: 15
        font.weight: UI.Style.weightSemiBold
        Accessible.role: Accessible.Heading
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        Heading {
            text: "Speed and memory"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                OptionRow {
                    width: parent.width
                    title: "When no Windows app is open"
                    // The board's line, with the 15 minutes the service now waits
                    // before saving to disk.
                    description: "Kept ready in memory for short breaks, saved to disk after 15 minutes or on battery"

                    UI.ComboBox {
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

                OptionRow {
                    width: parent.width
                    title: "Memory Windows mode can use"
                    description: (Setup.memoryLabel.length > 0 ? "This PC has " + Setup.memoryLabel + ". " : "")
                        + "More helps big spreadsheets. Unused memory is handed back."

                    UI.ComboBox {
                        Accessible.name: "Memory limit"
                        model: Setup.memoryChoices
                        currentIndex: model.findIndex(o => o.value === Setup.memoryGb)
                        onActivated: Setup.memoryGb = currentValue
                    }
                }

                OptionRow {
                    width: parent.width
                    visible: Setup.hyperThreadingAvailable
                    title: "Use every processor thread"
                    description: "Faster heavy apps, one less CPU-flaw protection for the whole PC. Needs a restart."

                    UI.Toggle {
                        text: checked ? "On" : "Off"
                        checked: Setup.hyperThreading
                        onToggled: Setup.hyperThreading = checked
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        Heading {
            text: "Your files"
        }

        UI.Card {
            Layout.fillWidth: true

            OptionRow {
                width: parent.width
                title: "Folders Windows apps can see"
                description: "Word and Excel save straight into these"

                Repeater {
                    model: Setup.folders

                    Rectangle {
                        required property string modelData
                        implicitWidth: folderName.implicitWidth + 20
                        implicitHeight: 24
                        radius: 12
                        color: UI.Style.tone(0.061)

                        UI.Label {
                            id: folderName
                            anchors.centerIn: parent
                            text: parent.modelData
                            size: 12
                            color: UI.Style.accentText
                        }
                    }
                }
            }
        }
    }

    // Windows' own licence screen is skipped by the answer file, and the
    // Office Deployment Tool's by its configuration, so this is where the
    // terms are accepted; nothing starts until they are.
    footerLeading: Item {
        anchors.fill: parent

        MouseArea {
            x: -10
            width: terms.width + 20
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            onClicked: Setup.termsAccepted = !Setup.termsAccepted
        }

        Row {
            id: terms
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            UI.CheckBox {
                anchors.verticalCenter: parent.verticalCenter
                checked: Setup.termsAccepted
                onToggled: Setup.termsAccepted = checked
                Accessible.name: termsText.plain
            }

            UI.Label {
                id: termsText
                readonly property string plain: "I accept Microsoft's licence terms for Windows 11" + (Setup.office ? " and Office" : "")
                anchors.verticalCenter: parent.verticalCenter
                // Rich text, for links in the accent without an underline.
                textFormat: Text.RichText
                size: 13
                elide: Text.ElideNone
                text: "I accept Microsoft's licence terms for "
                    + page.link("https://www.microsoft.com/en-us/useterms/retail/windows/11/useterms_retail_windows_11_english.htm", "Windows 11")
                    + (Setup.office ? " and " + page.link("https://www.microsoft.com/en-gb/useterms", "Office") : "")
                onLinkActivated: link => Setup.openUrl(link)

                HoverHandler {
                    cursorShape: termsText.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }

    buttons: [
        UI.Button {
            text: "Back"
            enabled: !page.busy
            onClicked: page.back()
        },
        UI.Button {
            kind: "primary"
            text: page.mainText
            enabled: Setup.canStart && !page.busy
            busy: page.busy
            onClicked: page.confirmed()
        }
    ]
}
