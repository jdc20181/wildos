import QtQuick
import QtQuick.Layouts
import WildOS.Welcome
import WildOS.UI as UI

// Step 7, from FirstWinApps.dc.html (3.9). The programs come from the
// Windows drive's registry and Program Files (WindowsPrograms); ticked ones
// are set up in the background after Finish: Store replacements through the
// Store's installer, others from a fresh copy of their own installer handed
// to the App Bridge. Windows mode is only planned, after its licence check:
// the background Windows tier itself is not built yet, and the card says so.
StepPage {
    id: page

    signal back()
    signal finished()

    // Not called "root": Main.qml handles this page's signals with root.finish(),
    // and a property of that name here would shadow Main's id.
    readonly property string windowsRoot: WindowsImport.windowsRoot
    readonly property string scanState: WindowsPrograms.state
    readonly property bool finishing: FirstBoot.state === "finishing" || FirstBoot.state === "signing-in"
    readonly property var modeNames: WindowsPrograms.windowsModeNames

    // The list scrolls inside the sheet rather than growing it past the
    // artboard's 720px.
    implicitHeight: 0
    gap: 18
    title: "Your Windows programs"
    subtitle: scanState === "none"
        ? "There's no Windows drive to look at, so there are no programs to bring across. Windows programs you download later open on Wild OS too."
        : "We found these programs on your Windows drive. Choose which to set up here; we fetch a fresh installer for each, so nothing is copied from Windows."
    note: WindowsImport.planning && FirstBoot.state === "failed" ? FirstBoot.error
        : FirstBoot.state === "signing-in" ? "Signing you in."
        : "Programs install in the background. You can start using Wild OS straight away."
    noteColor: WindowsImport.planning && FirstBoot.state === "failed" ? UI.Style.danger : UI.Style.inkMuted

    function look() {
        if (windowsRoot.length > 0) {
            WindowsPrograms.scan(windowsRoot, WindowsImport.userHome);
        } else if (WindowsImport.state === "found") {
            // Skipped in step 5, which closed the drive: open it again.
            WindowsImport.scan();
        } else if (WindowsImport.state !== "scanning") {
            WindowsPrograms.scan("", "");
        }
    }

    Component.onCompleted: {
        if (scanState === "" || scanState === "none") {
            look();
        }
        focusQuietly(finishButton);
    }
    onWindowsRootChanged: if (windowsRoot.length > 0 && scanState !== "done") look()

    Connections {
        target: WindowsImport
        function onStateChanged() {
            if (WindowsImport.state !== "scanning" && WindowsImport.state !== "found") {
                WindowsPrograms.scan("", "");
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: page.scanState !== "none"

        Column {
            width: parent.width

            UI.ListRow {
                visible: page.scanState !== "done" || WindowsPrograms.programs.length === 0
                minHeight: 52
                title: page.scanState === "done" ? "No programs found" : "Looking for your programs"
                description: page.scanState === "done" ? "Windows on this drive has only its own programs." : "Reading what's installed on Windows. This takes a few seconds."

                UI.Spinner {
                    visible: page.scanState !== "done"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Repeater {
                model: page.scanState === "done" ? WindowsPrograms.programs : []

                UI.ListRow {
                    id: row
                    required property var modelData
                    required property int index
                    minHeight: 52
                    divider: index > 0
                    interactive: modelData.selectable
                    title: modelData.name
                    description: modelData.detail
                    descriptionColor: UI.Style.inkAlpha(0.70, 0.64)
                    Accessible.name: modelData.name + ", " + chip.text
                    onClicked: WindowsPrograms.setChecked(modelData.key, !modelData.checked)

                    leading: UI.CheckBox {
                        enabled: row.modelData.selectable
                        checked: row.modelData.checked
                        focusPolicy: Qt.NoFocus
                        onClicked: WindowsPrograms.setChecked(row.modelData.key, checked)
                    }

                    // 24px pill, 0 10px, 12px: accent for Windows mode,
                    // green for the desktop, amber for not supported.
                    Rectangle {
                        id: chip
                        readonly property string kind: row.modelData.kind
                        readonly property string text: kind === "windows" ? "Uses Windows mode"
                            : kind === "desktop" ? "Runs on the desktop"
                            : kind === "unsupported" ? "Not supported"
                            : "Not tried yet"
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: chipLabel.implicitWidth + 20
                        implicitHeight: 24
                        radius: 12
                        color: kind === "windows" ? UI.Style.accentAlpha(0.16, 0.12)
                            : kind === "desktop" ? UI.Style.successSoft
                            : kind === "unsupported" ? UI.Style.warningSoft
                            : UI.Style.tone(0.07)

                        UI.Label {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.text
                            size: 12
                            color: chip.kind === "windows" ? UI.Style.accentText
                                : chip.kind === "desktop" ? UI.Style.success
                                : chip.kind === "unsupported" ? UI.Style.warning
                                : UI.Style.inkStatus
                        }
                    }
                }
            }
        }
    }

    UI.Label {
        Layout.fillWidth: true
        Layout.topMargin: -8
        visible: page.scanState === "done" && !WindowsPrograms.registryRead
        text: "Windows's list of installed programs couldn't be read, so these come from its Program Files folder."
        size: 12
        color: UI.Style.inkMuted
        wrapMode: Text.WordWrap
    }

    // Set up Windows mode: 14px 18px, the shield, 14px title over a 12px
    // line, and a 34px Set up button.
    UI.Card {
        Layout.fillWidth: true
        visible: page.scanState === "done"
        leftPadding: 18
        rightPadding: 18
        topPadding: 14
        bottomPadding: 14

        RowLayout {
            width: parent.width
            spacing: 14

            UI.Icon {
                name: "windowsapps"
                size: 22
                color: UI.Style.accentText
            }

            ColumnLayout {
                id: modeText
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: "Set up Windows mode"
                    size: 14
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: WindowsPrograms.licence.length > 0 ? WindowsPrograms.licenceText
                        : (page.modeNames.length > 0 ? "Needed for " + page.joined(page.modeNames) + ". " : "For Windows programs that don't run on the desktop. ")
                          + "Uses your own Windows licence (we check it first) and about 20 GB of space."
                    size: 12
                    lineHeight: 1.45
                    color: WindowsPrograms.licence === "none" ? UI.Style.warning : UI.Style.inkMuted
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }

            UI.Button {
                implicitHeight: 34
                visible: WindowsPrograms.licence !== "firmware" && WindowsPrograms.licence !== "registry"
                text: WindowsPrograms.licence === "none" ? "Check again" : "Set up"
                busy: WindowsPrograms.licence === "checking"
                onClicked: WindowsPrograms.checkLicence()
            }

            UI.Icon {
                visible: WindowsPrograms.licence === "firmware" || WindowsPrograms.licence === "registry"
                name: "checkmark"
                size: 16
                strokeWidth: 2.4
                color: UI.Style.success
            }
        }
    }

    function joined(names) {
        if (names.length <= 1) {
            return names.join("");
        }
        return names.slice(0, -1).join(", ") + " and " + names[names.length - 1];
    }

    buttons: [
        UI.Button {
            text: "Back"
            enabled: !page.finishing
            onClicked: page.back()
        },
        UI.Button {
            id: finishButton
            text: "Finish"
            kind: "primary"
            busy: page.finishing
            enabled: page.scanState === "done" || page.scanState === "none"
            onClicked: page.finished()
        }
    ]
}
