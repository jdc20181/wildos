import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import WildOS.WinMode.Backend

// 9.1, Check this PC (WinModeCheck.dc). Every row is a real check: CPUID
// and /dev/kvm, the memory fitted, the space on the home disk, the
// connection, and the licence table in the firmware.
Page {
    id: page

    signal next()
    signal later()
    signal problem(string kind)

    readonly property bool ready: !Setup.checking && Setup.virtualisation === "ok" && Setup.memoryState !== "short" && Setup.spaceOk

    title: "Set up Windows mode"
    text: "Run Windows programs like Word, Excel and Photoshop in their own windows, next to your other apps. Windows stays hidden in the background and only wakes when you open one."
    note: page.ready || Setup.checking ? "Windows downloads straight from Microsoft and uses your own licence."
        : Setup.virtualisation !== "ok" ? "Windows mode can't start until virtualisation is switched on."
        : !Setup.spaceOk ? "Windows mode needs about " + Setup.needLabel + " free to set up."
        : "This PC doesn't have enough memory for Windows mode."

    Banner {
        tone: Setup.licence === "firmware" ? "success" : Setup.licence === "checking" ? "accent" : "warning"
        title: {
            switch (Setup.licence) {
            case "checking": return "Looking for a Windows licence";
            case "firmware": return "Windows licence found";
            case "inuse": return "Windows is still installed on this PC";
            case "error": return "The licence check didn't finish";
            default: return "No Windows licence built into this PC";
            }
        }
        text: {
            switch (Setup.licence) {
            case "checking": return "Checking this PC's firmware.";
            case "firmware":
                return Setup.edition === "pro" ? "Windows 11 Pro is built into this PC, so Windows mode uses it. There is nothing to buy."
                     : Setup.edition === "home" ? "Windows 11 Home is built into this PC, so Windows mode uses it. There is nothing to buy."
                     : "A Windows licence is built into this PC, so Windows mode uses it. Windows confirms whether it's Home or Pro while it sets itself up.";
            case "inuse": return "Its licence is built into this PC. On the next step, choose to use it for Windows mode as well or add a product key.";
            case "error": return "You can still carry on and enter a product key.";
            default: return "You can enter a product key on the next step.";
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            CheckRow {
                title: "Virtualisation"
                state: Setup.virtualisation === "ok" ? "ok" : "failed"
                detail: {
                    switch (Setup.virtualisation) {
                    case "ok": return "Switched on in this PC's firmware";
                    case "unsupported": return "This PC's processor can't run Windows mode";
                    case "noaccess": return "Only available at the PC's own screen";
                    default: return "Switched off in this PC's firmware";
                    }
                }
                value: Setup.technology
            }

            CheckRow {
                title: "Memory"
                state: Setup.memoryState === "ok" ? "ok" : Setup.memoryState === "low" ? "warning" : "failed"
                detail: Setup.memoryState === "short" ? Setup.memoryLabel + ". Windows mode needs at least 8 GB."
                      : Setup.memoryState === "low" ? Setup.memoryLabel + ". Works, but your PC may slow down while Windows apps are open."
                      : Setup.memoryLabel + ". Windows mode borrows about 2 GB while you use it"
                value: Setup.memoryLabel
            }

            CheckRow {
                title: "Space"
                state: Setup.checking && Setup.freeLabel.length === 0 ? "checking" : Setup.spaceOk ? "ok" : "failed"
                detail: "About " + Setup.needLabel + " needed"
                value: Setup.freeLabel.length > 0 ? Setup.freeLabel + " free" : ""
            }

            CheckRow {
                title: "Internet"
                state: Setup.internet === "checking" ? "checking" : Setup.internet === "offline" ? "warning" : "ok"
                detail: Setup.internet === "offline" ? "Connect to download Windows from Microsoft, about 6 GB"
                      : Setup.internet === "metered" ? "On a metered connection. Windows is about 6 GB"
                      : "Windows downloads from Microsoft, about 6 GB"
                value: Setup.internet === "offline" ? "Not connected" : Setup.internet === "checking" ? "" : "Connected"
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        UI.Label {
            text: "What happens next"
            size: 15
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 10

            Repeater {
                model: ["Windows downloads from Microsoft", "It installs and activates itself", "We strip out adverts and tracking", "Your apps appear in Start"]

                Rectangle {
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: stepText.implicitHeight + 24
                    radius: UI.Style.radiusMd
                    color: UI.Style.surfaceRest
                    border.width: 1
                    border.color: UI.Style.borderSubtle

                    Column {
                        id: stepText
                        x: 14
                        y: 12
                        width: parent.width - 28
                        spacing: 4

                        UI.Label {
                            text: index + 1
                            size: 12
                            font.weight: Font.DemiBold
                            color: UI.Style.accentText
                        }

                        UI.Label {
                            width: parent.width
                            text: modelData
                            size: 13
                            wrapMode: Text.Wrap
                            elide: Text.ElideNone
                            lineHeight: 1.4
                        }
                    }
                }
            }
        }
    }

    buttons: [
        UI.Button {
            text: "Not now"
            onClicked: page.later()
        },
        UI.Button {
            kind: "primary"
            text: "Continue"
            enabled: page.ready
            busy: Setup.checking
            onClicked: page.next()
        }
    ]

    // When a check finishes with something setup can't get past, say so in
    // the problem's own card (9.7), once.
    property bool told: false
    Connections {
        target: Setup
        function onChecksChanged() {
            if (Setup.checking || page.told) {
                return;
            }
            if (Setup.virtualisation === "off") {
                page.told = true;
                page.problem("virtualisation");
            } else if (!Setup.spaceOk) {
                page.told = true;
                page.problem("space");
            }
        }
    }
}
