import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Settings
import WildOS.UI as UI

// Windows apps > Windows mode, laid out as WinModeSettings.dc. Every value
// is the Windows mode service's own (WindowsMode, src/windowsmode.h); when
// Windows mode is not installed or not set up the page says so and offers
// the one button that helps.
PageScroll {
    id: page

    readonly property var status: WindowsMode.status
    readonly property var licence: status.licence || ({})
    readonly property var options: status.options || ({})
    readonly property bool ready: WindowsMode.state === "ready"
    readonly property string vm: status.vm || ""
    property string message

    function show(result) {
        page.message = result || "";
    }

    UI.Card {
        Layout.fillWidth: true
        visible: !page.ready

        Column {
            width: parent.width

            UI.ListRow {
                title: !WindowsMode.installed ? "Windows mode isn't installed"
                     : WindowsMode.state === "setup" ? "Setting up Windows mode"
                     : WindowsMode.state === "failed" ? "Windows mode couldn't finish setting up"
                     : "Windows mode isn't set up"
                description: !WindowsMode.installed ? "Get Windows mode from the Store to run Windows programs like Office and Photoshop in their own windows."
                     : WindowsMode.state === "setup" ? (page.status.percent || 0) + "% done. " + (page.status.left || "")
                     : WindowsMode.state === "failed" ? ((page.status.problem || {}).text || "Nothing was left half installed.")
                     : "Run Windows programs like Office and Photoshop in their own windows, using the Windows licence built into this PC."

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: WindowsMode.installed
                    kind: "primary"
                    text: WindowsMode.state === "setup" ? "Show progress" : WindowsMode.state === "failed" ? "Try again" : "Set up"
                    onClicked: WindowsMode.openSetup()
                }

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !WindowsMode.installed && System.storeApp.length > 0
                    text: "Open the Store"
                    onClicked: Qt.openUrlExternally("appstream://org.wildos.winmode")
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: page.ready
        spacing: 12

        Repeater {
            model: [
                {
                    label: "Status",
                    value: page.vm === "running" ? "Running" : page.vm === "paused" ? "Paused" : page.vm === "saved" ? "Saved to disk" : "Off",
                    detail: page.vm === "running" ? ((page.status.apps || []).filter(a => a.open).length + " Windows apps open")
                          : page.vm === "paused" ? "Opens in about 1 second"
                          : page.vm === "saved" ? "Opens in about 5 seconds" : "Opens in about 30 seconds"
                },
                {
                    label: "Memory",
                    value: page.status.memoryHeld > 0 ? WindowsMode.size(page.status.memoryHeld) + " held" : "None held",
                    detail: page.vm === "paused" ? "No CPU while paused" : page.vm === "running" ? "Handed back when unused" : "Nothing in use"
                },
                {
                    label: "Space",
                    value: WindowsMode.size(page.status.diskBytes) || "Unknown",
                    detail: page.options.office ? "Windows and Microsoft 365" : "Windows"
                }
            ]

            UI.Card {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: tileColumn.implicitHeight + 28

                Column {
                    id: tileColumn
                    x: 16
                    y: 14
                    width: parent.width - 32
                    spacing: 4

                    UI.Label {
                        text: modelData.label
                        size: 12
                        color: UI.Style.inkMuted
                    }

                    UI.Label {
                        text: modelData.value
                        size: 18
                        font.weight: Font.DemiBold
                    }

                    UI.Label {
                        text: modelData.detail
                        size: 12
                        color: UI.Style.inkMuted
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.ready

        Column {
            width: parent.width

            UI.ListRow {
                minHeight: 54
                title: "Licence"
                description: {
                    const edition = page.licence.edition === "home" ? "Windows 11 Home" : page.licence.edition === "pro" ? "Windows 11 Pro" : "Windows 11";
                    if (page.licence.source === "none") {
                        return edition + ", not activated";
                    }
                    const how = page.licence.source === "key" ? "your product key ending " + (page.licence.keyEnd || "") : "this PC's built-in licence";
                    return edition + (page.licence.activated === true ? ", activated with " : ", to be activated with ") + how;
                }

                UI.Chip {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.licence.activated === true ? "Activated" : "Not activated"
                    tone: page.licence.activated === true ? "success" : "warning"
                }

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.licence.source === "none" ? "Add a key" : "Change key"
                    onClicked: keyDialog.open()
                }
            }

            UI.ListRow {
                minHeight: 54
                title: "When no Windows app is open"
                description: {
                    switch (page.options.whenIdle) {
                    case "memory": return "Kept ready in memory, using no processor time";
                    case "disk": return "Saved to disk after five minutes, so its memory is free";
                    case "shutdown": return "Windows shuts down after five minutes";
                    default: return "Automatic: kept in memory while plugged in, saved to disk on battery";
                    }
                }

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "When no Windows app is open"
                    model: [
                        { value: "automatic", text: "Automatic" },
                        { value: "memory", text: "Keep ready in memory" },
                        { value: "disk", text: "Save to disk" },
                        { value: "shutdown", text: "Shut down Windows" }
                    ]
                    currentIndex: model.findIndex(o => o.value === (page.options.whenIdle || "automatic"))
                    onActivated: index => page.show(WindowsMode.setOption("whenIdle", model[index].value))
                }
            }

            UI.ListRow {
                minHeight: 54
                title: "Memory Windows mode can use"
                // Windows only takes a new limit when it starts from off, so
                // until then the row says plainly that the change is waiting.
                description: page.status.memoryPending
                    ? "Windows gets " + (page.options.memoryGb || 4) + " GB when it next starts. Wild OS restarts it by itself once no Windows app is open."
                    : "Unused memory is handed back to Wild OS. A change applies once no Windows app is open, when Windows restarts."

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Memory limit"
                    model: WindowsMode.memoryChoices
                    currentIndex: model.findIndex(o => o.value === (page.options.memoryGb || 4))
                    onActivated: index => page.show(WindowsMode.setOption("memoryGb", String(model[index].value)))
                }
            }

            // A shortcut: the switch lives in System > Power and battery, because
            // it speeds up the whole PC, not only Windows mode.
            UI.ListRow {
                minHeight: 54
                visible: Processor.threadsAvailable
                title: "Use every processor thread"
                description: Processor.everyThreadLocked
                    ? "Off at Strict and Maximum protection."
                    : "Faster Windows apps and games. Set in Power and battery."
                status: Processor.everyThread && !Processor.everyThreadLocked ? "On" : "Off"

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Change"
                    onClicked: Navigator.open("power")
                }
            }

            UI.ListRow {
                minHeight: 54
                title: "Folders Windows apps can see"
                description: {
                    const list = page.options.folders || [];
                    if (list.length === 0) {
                        return "None. Windows apps can't open or save your files.";
                    }
                    return list.length === 1 ? list[0] : list.slice(0, -1).join(", ") + " and " + list[list.length - 1];
                }

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Change"
                    onClicked: foldersDialog.open()
                }
            }

            UI.ListRow {
                minHeight: 54
                title: "Camera and microphone"
                description: {
                    switch (page.options.devices) {
                    case "always": return "Call apps in Windows mode always get them";
                    case "never": return "Windows apps never get them";
                    default: return "Ask each time a Windows app wants them, like any other app";
                    }
                }

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Camera and microphone"
                    model: [
                        { value: "ask", text: "Ask each time" },
                        { value: "always", text: "Always allow" },
                        { value: "never", text: "Never allow" }
                    ]
                    currentIndex: model.findIndex(o => o.value === (page.options.devices || "ask"))
                    onActivated: index => page.show(WindowsMode.setOption("devices", model[index].value))
                }
            }

            UI.ListRow {
                minHeight: 54
                title: "Windows and Office updates"
                description: "Monthly updates wait a week, so Wild OS can hold back one that breaks Windows apps."
                    + (page.status.officeVersion ? " Microsoft 365 " + page.status.officeVersion + "." : "")

            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.ready

        Column {
            width: parent.width

            UI.ListRow {
                minHeight: 54
                title: "Troubleshooting"
                description: "Open the whole Windows desktop in a window, for Windows' own settings or an installer that needs it. Your Windows apps join it until you close it."

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    UI.Button {
                        text: "Windows desktop"
                        onClicked: page.show(WindowsMode.openDesktop())
                    }

                }
            }
        }
    }

    UI.Label {
        Layout.fillWidth: true
        visible: page.message.length > 0
        text: page.message
        size: 13
        color: UI.Style.warning
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    // The dialog stays open while the key goes in, which can take a few
    // minutes when Windows has to start first, and says how it went.
    UI.Dialog {
        id: keyDialog
        title: "Change the Windows product key"
        text: "Windows checks the key with Microsoft and activates with it. The key from this PC's firmware stays where it is."
        closePolicy: WindowsMode.activating ? T.Popup.NoAutoClose : T.Popup.CloseOnEscape

        property string keyProblem

        onOpened: {
            keyField.text = "";
            keyProblem = "";
        }

        Connections {
            target: WindowsMode
            function onProductKeyDone(answer) {
                if (answer === "") {
                    keyDialog.close();
                    page.show("");
                } else {
                    keyDialog.keyProblem = answer;
                }
            }
        }

        UI.TextField {
            id: keyField
            Layout.fillWidth: true
            enabled: !WindowsMode.activating
            placeholderText: "XXXXX-XXXXX-XXXXX-XXXXX-XXXXX"
            error: text.length >= 25 && !WindowsMode.keyShapeValid(text)
            Accessible.name: "Windows product key"
        }

        UI.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: WindowsMode.activating ? "Activating Windows. If Windows was asleep it starts first, which can take a few minutes."
                                         : keyDialog.keyProblem
            size: 13
            color: WindowsMode.activating ? UI.Style.inkStatus : UI.Style.warning
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            Accessible.role: Accessible.AlertMessage
        }

        buttons: [
            UI.Button {
                text: "Cancel"
                enabled: !WindowsMode.activating
                onClicked: keyDialog.close()
            },
            UI.Button {
                text: "Use this PC's licence"
                visible: page.licence.source !== "firmware"
                enabled: !WindowsMode.activating
                onClicked: {
                    keyDialog.keyProblem = "";
                    WindowsMode.setProductKey("firmware");
                }
            },
            UI.Button {
                kind: "primary"
                text: WindowsMode.activating ? "Activating" : "Use this key"
                enabled: !WindowsMode.activating && WindowsMode.keyShapeValid(keyField.text)
                onClicked: {
                    keyDialog.keyProblem = "";
                    WindowsMode.setProductKey(keyField.text);
                }
            }
        ]
    }

    UI.Dialog {
        id: foldersDialog
        title: "Folders Windows apps can see"
        text: "Windows apps can open and save files in these folders. A change applies the next time Windows starts."

        property var chosen: []
        onOpened: chosen = (page.options.folders || []).slice()

        Repeater {
            model: ["Documents", "Downloads", "Pictures", "Desktop", "Music", "Videos"]

            UI.CheckBox {
                required property string modelData
                text: modelData
                checked: foldersDialog.chosen.indexOf(modelData) >= 0
                onToggled: {
                    const list = foldersDialog.chosen.filter(f => f !== modelData);
                    if (checked) {
                        list.push(modelData);
                    }
                    foldersDialog.chosen = list;
                }
            }
        }

        buttons: [
            UI.Button {
                text: "Cancel"
                onClicked: foldersDialog.close()
            },
            UI.Button {
                kind: "primary"
                text: "Save"
                onClicked: {
                    const order = ["Documents", "Downloads", "Pictures", "Desktop", "Music", "Videos"];
                    page.show(WindowsMode.setOption("folders", order.filter(f => foldersDialog.chosen.indexOf(f) >= 0).join(",")));
                    foldersDialog.close();
                }
            }
        ]
    }

    UI.Dialog {
        id: repairDialog
        title: "Repair Windows mode?"
        text: "Windows goes back to how it was when setup finished. Programs and settings changed in Windows since then are lost; your documents stay in your folders."
        buttons: [
            UI.Button {
                text: "Cancel"
                onClicked: repairDialog.close()
            },
            UI.Button {
                kind: "primary"
                text: "Repair"
                onClicked: {
                    repairDialog.close();
                    page.show(WindowsMode.repair());
                }
            }
        ]
    }

    UI.Dialog {
        id: removeDialog
        title: "Remove Windows mode?"
        text: "Windows, its programs and their Start entries are deleted, which frees " + (WindowsMode.size(page.status.diskBytes) || "the space they use") + ". Your documents stay in your folders."
        buttons: [
            UI.Button {
                text: "Cancel"
                onClicked: removeDialog.close()
            },
            UI.Button {
                kind: "primary"
                danger: true
                text: "Remove"
                onClicked: {
                    removeDialog.close();
                    page.show(WindowsMode.remove());
                }
            }
        ]
    }
}
