pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as P5Support
import WildOS.UI as UI

// The Updates icon beside Protection on the taskbar and its flyout: whether
// updates are waiting, checking for them, and installing them through
// Settings, which asks for the password. Everything shown comes from
// /usr/libexec/wildos/update-status, which asks PackageKit, so it always
// agrees with Settings > Updates and the Store.
PlasmoidItem {
    id: updates

    readonly property string libexec: "/usr/libexec/wildos/"

    // key -> value, as printed by update-status.
    property var status: ({})
    readonly property bool loaded: Object.keys(status).length > 0
    property real now: Date.now()
    // Check now was pressed here and update-status is still refreshing.
    property bool checkRunning: false

    readonly property string phase: !loaded ? "loading" : checkRunning && status.state !== "installing" ? "checking" : status.state
    readonly property int count: number(status.updates)
    readonly property int securityCount: number(status.security)
    readonly property bool busy: phase === "checking" || phase === "installing"
    readonly property bool byAuto: status.by === "auto"
    readonly property bool restartNeeded: status.restart === "yes"
    readonly property bool ready: phase === "available"
    readonly property bool failed: phase === "error"
    // A restart only stands out once nothing else is going on.
    readonly property bool restartShown: restartNeeded && (phase === "current" || phase === "unchecked")
    readonly property bool canCheck: loaded && phase !== "na" && !busy

    function number(value: var): int {
        const n = parseInt(value, 10);
        return isNaN(n) ? 0 : n;
    }

    function plural(n: int, one: string, many: string): string {
        return n === 1 ? "1 " + one : n + " " + many;
    }

    // Times in the regional format chosen in Settings > Language and region,
    // on the 12 or 24-hour clock chosen in Settings > Date and time,
    // as the clock writes them (org.wildos.clock's Formats.js).
    function timeText(when: var): string {
        return Qt.locale().toString(when, UI.TimeFormat.shortFormat);
    }

    function dateText(when: var): string {
        return Qt.locale("en_GB").toString(when, Qt.locale().dateFormat(Locale.ShortFormat));
    }

    // "just now", "10 minutes ago", "today at 13:10", "yesterday at 13:10",
    // then the date, as Settings > Updates words it.
    function ago(when: var): string {
        const minutes = Math.floor((now - when.getTime()) / 60000);
        if (minutes < 1) {
            return "just now";
        }
        if (minutes < 60) {
            return plural(minutes, "minute", "minutes") + " ago";
        }
        const today = new Date(now);
        if (today.toDateString() === when.toDateString()) {
            return "today at " + timeText(when);
        }
        if (new Date(now - 86400000).toDateString() === when.toDateString()) {
            return "yesterday at " + timeText(when);
        }
        return dateText(when) + " at " + timeText(when);
    }

    readonly property string checkedText: {
        const at = number(status.checked);
        return at > 0 ? "Last checked " + ago(new Date(at * 1000)) : "Not checked yet";
    }

    readonly property string readyText: securityCount > 0
        ? plural(securityCount, "security fix is", "security fixes are") + " ready to install"
        : plural(count, "update is", "updates are") + " ready to install"

    // One line for the tooltip and the screen reader.
    readonly property string summary: {
        switch (phase) {
        case "loading": return "Updates";
        case "na": return status.why === "live" ? "Updates start once Wild OS is installed" : "Updates are not available";
        case "installing": return byAuto ? "Installing security updates" : "Installing updates";
        case "checking": return "Checking for updates";
        case "error": return "Couldn't check for updates";
        case "available": return plural(count, "update", "updates") + " ready";
        case "unchecked": return number(status.checked) > 0 ? "Updates not checked lately" : "Updates not checked yet";
        default: return restartNeeded ? "Restart to finish updating" : "Wild OS is up to date";
        }
    }

    readonly property string title: {
        switch (phase) {
        case "loading": return "Updates";
        case "na": return status.why === "live" ? "Updates start once Wild OS is installed" : "Updates are not available";
        case "installing": return byAuto ? "Installing security updates" : "Installing updates";
        case "checking": return "Checking for updates";
        case "error": return "Couldn't check for updates";
        case "available": return readyText;
        case "unchecked": return number(status.checked) > 0 ? "Not checked for updates lately" : "Not checked for updates yet";
        default: return restartNeeded ? "Restart to finish updating" : "You're up to date";
        }
    }

    readonly property string detail: {
        switch (phase) {
        case "loading": return "";
        case "na": return status.why === "live" ? "Wild OS keeps itself up to date once it is on your computer." : "The update service is not installed on this computer.";
        case "installing": return byAuto ? "Small fixes install by themselves. You can keep working." : "You can keep working while they install.";
        case "checking": return "This can take a minute on a slow connection.";
        case "error": return status.error || "Try again in a few minutes.";
        case "available": return "Install " + (count === 1 ? "it" : "them") + (securityCount > 0 ? " now to stay protected." : " when it suits you.");
        case "unchecked": return "Check now to see what's new.";
        default: return restartNeeded ? "Some updates only take effect after a restart." : "Nothing is waiting to install.";
        }
    }

    P5Support.DataSource {
        id: checker
        engine: "executable"
        readonly property string command: updates.libexec + "update-status"
        // Each read is its own source (a numbered comment): the engine hands
        // a source it already has its last output.
        property string current: command
        connectedSources: [current]
        onNewData: (source, data) => updates.receiveStatus(data.stdout)
    }

    function receiveStatus(stdout: string): void {
        const next = {};
        for (const line of String(stdout).trim().split("\n")) {
            const at = line.indexOf("=");
            if (at > 0) {
                next[line.slice(0, at)] = line.slice(at + 1);
            }
        }
        if (next.state === undefined) {
            return;
        }
        status = next;
        now = Date.now();
    }

    property int readCount: 0

    function refreshStatus(): void {
        checker.disconnectSource(checker.current);
        readCount += 1;
        checker.current = checker.command + " #" + readCount;
        checker.connectSource(checker.current);
    }

    // Every few seconds while something is checking or installing, the
    // flyout is open or Install was just pressed (Settings takes a moment to
    // start), so the icon keeps up; every half minute otherwise, which is
    // cheap because update-status only asks PackageKit again after the lists
    // or the installed packages change.
    property bool justAsked: false

    Timer {
        id: askedWindow
        interval: 120000
        onTriggered: updates.justAsked = false
    }

    Timer {
        running: true
        repeat: true
        interval: updates.busy || updates.expanded || updates.justAsked ? 4000 : 30000
        onTriggered: updates.refreshStatus()
    }

    // One-off commands; a check answers with the status once it is done.
    P5Support.DataSource {
        id: shell
        engine: "executable"
        onNewData: (source, data) => {
            disconnectSource(source);
            if (source.indexOf("update-status check") >= 0) {
                updates.checkRunning = false;
                updates.receiveStatus(data.stdout);
            }
        }
    }

    property int runCount: 0

    function run(command: string): void {
        runCount += 1;
        shell.connectSource(command + " #" + runCount);
    }

    function checkNow(): void {
        if (!canCheck || checkRunning) {
            return;
        }
        checkRunning = true;
        run(libexec + "update-status check");
    }

    // Settings ships with Wild OS, but if it has gone the click still says
    // something rather than doing nothing.
    function openSettings(page: string): void {
        run("if command -v wildos-settings >/dev/null; then setsid -f wildos-settings --page " + page + " >/dev/null 2>&1; "
            + "else notify-send -a 'Wild OS' -i preferences-system 'Settings is missing' "
            + "'Reinstall it from the Store to change this.'; fi");
        expanded = false;
    }

    function openUpdates(): void {
        openSettings("update");
    }

    // Settings installs, so the password prompt, progress and any restart
    // it asks for are the ones Settings > Updates already shows.
    function install(): void {
        justAsked = true;
        askedWindow.restart();
        openSettings("update-install");
    }

    onExpandedChanged: () => {
        if (updates.expanded) {
            now = Date.now();
            refreshStatus();
        }
    }

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.icon: "system-software-update"
    Plasmoid.title: "Updates"
    Plasmoid.status: ready || failed ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    toolTipMainText: summary
    toolTipSubText: loaded && phase !== "na" ? checkedText : ""

    preferredRepresentation: compactRepresentation
    compactRepresentation: UpdateButton {
        updates: updates
    }
    fullRepresentation: Flyout {
        updates: updates
    }

    Component.onCompleted: {
        if (Plasmoid.hasOwnProperty("activationTogglesExpanded")) {
            Plasmoid.activationTogglesExpanded = true;
        }
    }
}
