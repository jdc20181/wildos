import QtQuick
import QtQuick.Layouts
import WildOS.Welcome
import WildOS.WinMode.Setup as WinMode
import WildOS.UI as UI

// Step 7, page 1, from FirstWinApps.dc.html (3.9): Windows mode, offered
// only when Windows is on this PC. The licence card and the checks are the
// setup window's own (9.1, WinMode.CheckList), run before this page shows.
// Anything not met turns Set up Windows mode off and leaves Not now.
StepPage {
    id: page

    // Set up Windows mode, or Not now.
    property bool setUp: WinMode.Setup.requirementsMet
    readonly property bool finishing: FirstBoot.state === "finishing" || FirstBoot.state === "signing-in"

    signal back()
    signal next()
    signal finished()

    title: "Use your Windows programs here?"
    subtitle: "Windows mode runs Windows programs, like Microsoft Office, in their own windows next to your other apps. Windows stays hidden and only wakes when you open one."
    gap: 16
    note: page.setUp ? "Next: choose options, including Office and your Windows licence."
        : WindowsImport.planning && FirstBoot.state === "failed" ? FirstBoot.error
        : FirstBoot.state === "signing-in" ? "Signing you in." : ""
    noteColor: WindowsImport.planning && FirstBoot.state === "failed" ? UI.Style.danger : UI.Style.inkMuted
    Component.onCompleted: focusQuietly(nextButton)

    // "about an hour" reads better than "about 66 minutes".
    function about(minutes) {
        return minutes >= 55 && minutes <= 75 ? "about an hour" : "about " + Math.round(minutes / 5) * 5 + " minutes";
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Accessible.role: Accessible.Grouping
        Accessible.name: "Set up Windows mode"

        WinMode.RadioCard {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            dense: true
            checked: page.setUp
            enabled: WinMode.Setup.requirementsMet
            title: "Set up Windows mode"
            chip: "Recommended"
            description: "About " + WinMode.Setup.needLabel + ". Sets itself up in the background in " + page.about(WinMode.Setup.setupMinutes)
                + ", and you can use Wild OS straight away."
            reason: WinMode.Setup.requirementsMet ? "" : "This PC doesn't meet everything below yet."
            onClicked: page.setUp = true
        }

        WinMode.RadioCard {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            dense: true
            checked: !page.setUp
            title: "Not now"
            description: "You can set it up any time in Settings, under Windows apps."
            onClicked: page.setUp = false
        }
    }

    WinMode.CheckList {
        spacing: page.gap
    }

    buttons: [
        UI.Button {
            text: "Back"
            enabled: !page.finishing
            onClicked: page.back()
        },
        UI.Button {
            id: nextButton
            kind: "primary"
            text: page.setUp ? "Next" : "Finish"
            busy: page.finishing
            onClicked: page.setUp ? page.next() : page.finished()
        }
    ]
}
