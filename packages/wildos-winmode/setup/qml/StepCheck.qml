import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.UI as UI
import WildOS.WinMode.Setup

// 9.1, Check this PC (WinModeCheck.dc): the licence card and the checks
// first run shows too (CheckList), and what happens next.
Page {
    id: page

    signal next()
    signal later()
    signal problem(string kind)

    readonly property bool ready: Setup.requirementsMet
    // A Windows 11 file the person already has, in place of the download.
    readonly property bool ownIso: Setup.isoState === "ok"

    title: "Set up Windows mode"
    text: "Run Windows programs like Word, Excel and Photoshop in their own windows, next to your other apps. Windows stays hidden in the background and only wakes when you open one."
    // Only a download can be matched to Microsoft's signature, so a file
    // someone brings is never called genuine, only Windows 11 Pro.
    note: page.ownIso && page.ready ? "Your file is checked to be Windows 11 Pro. Unlike a download, it can't be checked against Microsoft's signature."
        : page.ready || Setup.checking ? "Windows downloads straight from Microsoft and uses your own licence."
        : Setup.virtualisation !== "ok" ? "Windows mode can't start until virtualisation is switched on."
        : !Setup.spaceOk ? "Windows mode needs about " + Setup.needLabel + " free to set up."
        : !Setup.coresOk ? "Windows mode needs a processor with 4 cores or more."
        : Setup.internet === "offline" ? "Connect to the internet to download Windows."
        : "This PC doesn't have enough memory for Windows mode."

    CheckList {
        spacing: page.spacing
        isoLink: true
        onIsoRequested: isoDialog.open()
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        UI.Label {
            text: "What happens next"
            size: 15
            font.weight: UI.Style.weightSemiBold
            Accessible.role: Accessible.Heading
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 10

            Repeater {
                model: [page.ownIso ? "Windows comes from your file" : "Windows downloads from Microsoft", "It installs and activates itself",
                        "We strip out adverts and tracking", "Your apps appear in Start"]

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
                            font.weight: UI.Style.weightSemiBold
                            color: UI.Style.accentText
                        }

                        UI.Label {
                            width: parent.width
                            text: modelData
                            size: 13
                            wrapMode: Text.Wrap
                            elide: Text.ElideNone
                            lineHeightMode: Text.FixedHeight
                            lineHeight: Math.round(UI.Style.px(size) * 1.4)
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: isoDialog
        title: "Choose a Windows 11 file"
        nameFilters: ["Windows 11 files (*.iso *.ISO)"]
        currentFolder: Setup.downloadsFolder
        onAccepted: Setup.checkIso(selectedFile)
    }

    UI.Dialog {
        id: isoRefused
        title: "This file can't be used"
        buttons: [
            UI.Button {
                kind: "primary"
                text: "Choose another file"
                onClicked: {
                    isoRefused.close();
                    isoDialog.open();
                }
            },
            UI.Button {
                text: "Close"
                onClicked: isoRefused.close()
            }
        ]
    }

    Connections {
        target: Setup
        function onIsoRefused(text) {
            isoRefused.text = text;
            isoRefused.open();
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
