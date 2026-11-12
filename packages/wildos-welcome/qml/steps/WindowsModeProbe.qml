import QtQuick
import WildOS.WinMode.Setup as WinMode

// Starts Windows mode's checks as the welcome opens, so they are done by
// step 7, which only shows when Windows is found on this PC. Loaded by
// URL rather than as a type, so a PC without Windows mode's package still
// runs the welcome, without the step.
QtObject {
    readonly property bool checking: WinMode.Setup.checking || WinMode.Setup.licence === "checking"
    readonly property bool windowsFound: WinMode.Setup.windowsFound
    readonly property bool requirementsMet: WinMode.Setup.requirementsMet

    function request(): string {
        return WinMode.Setup.setupRequest(true);
    }

    // Outside the first boot session: "" once setup has started, or why not.
    function start(): string {
        return WinMode.Setup.startSetup(true);
    }

    Component.onCompleted: WinMode.Setup.begin("firstrun")
}
