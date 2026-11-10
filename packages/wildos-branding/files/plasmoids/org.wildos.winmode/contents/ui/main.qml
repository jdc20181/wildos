pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as P5Support

// Windows mode's status at the left end of the taskbar, where Windows 11
// puts its weather (boards 4.13 and 4.14): what Windows mode is doing in a
// word, the memory it holds on hover, and a panel with processor use, memory
// and the Windows apps open. Everything comes from "wildos-winmode status",
// so it only appears once this account has Windows mode set up.
PlasmoidItem {
    id: winmode

    readonly property string statusCommand: "wildos-winmode status --if-set-up"

    // The agent's and the system service's status (winmode/agent.py and
    // winmode/system.py), as last read.
    property var agent: null
    property var system: null
    // False when Windows mode is set up but its services didn't reply.
    property bool answering: true
    property real now: Date.now()

    // {t: ms, cpu: QEMU's processor seconds}, the last minute and a bit.
    property var samples: []
    // Used when QEMU's processor count can't be read.
    property int hostCores: 0

    readonly property var setupRecord: system ? system.setup : null
    readonly property bool setUp: !answering || (system !== null && (system.vm || "") !== ""
        && !(setupRecord && setupRecord.running) && (!setupRecord || setupRecord.stage === "done"))
    readonly property bool shown: setUp && Plasmoid.configuration.shown

    readonly property var openApps: agent && Array.isArray(agent.openApps) ? agent.openApps : systemWindows(system)
    readonly property int appCount: {
        const ids = {};
        for (const w of openApps) {
            ids[w.id] = true;
        }
        return Object.keys(ids).length;
    }

    // One of: running, paused, saved, off, starting, updating, attention.
    // Not "state": Item has one.
    readonly property string level: levelFor(system, agent, answering, openApps.length)
    readonly property string stateText: ({
        running: appCount === 0 ? "Running" : appCount === 1 ? "Running, 1 app" : "Running, " + appCount + " apps",
        paused: "Paused",
        saved: "Saved to disk",
        off: "Not running",
        starting: "Starting",
        updating: "Updating",
        attention: "Needs attention"
    })[level]
    readonly property string pillText: ({
        running: "Running", paused: "Paused", saved: "Saved", off: "Off",
        starting: "Starting", updating: "Updating", attention: "Needs attention"
    })[level]

    readonly property var qemu: system && system.qemu ? system.qemu : null
    readonly property real memoryHeld: qemu ? Number(qemu.rss_bytes) || 0 : 0
    readonly property real memoryLimit: system ? Number(system.memory_bytes) || 0 : 0
    readonly property int processors: qemu && qemu.vcpus > 0 ? qemu.vcpus : Math.max(1, hostCores)
    readonly property var lastApp: agent && agent.lastApp && /^[a-z0-9-]+$/.test(agent.lastApp.id) ? agent.lastApp : null
    readonly property bool canSave: system !== null && system.save_to_disk === true
    readonly property bool busyAction: system !== null && !!system.action

    function levelFor(sys: var, ag: var, ok: bool, windows: int): string {
        if (!ok || !sys) {
            return "attention";
        }
        if (sys.state === "Error" || sys.error) {
            return "attention";
        }
        const busy = Array.isArray(sys.busy) ? sys.busy : [];
        if (busy.includes("update") || busy.includes("maintenance")) {
            return "updating";
        }
        if (ag && ag.starting && windows === 0) {
            return "starting";
        }
        return ({ Running: "running", Paused: "paused", Saved: "saved" })[sys.state] || "off";
    }

    // Without the agent, the system service's list still says what is open,
    // by program name.
    function systemWindows(sys: var): var {
        if (!sys || !Array.isArray(sys.windows)) {
            return [];
        }
        return sys.windows.filter(w => w && !w.owner && w.app).map(w => ({
            id: w.app, name: w.app.charAt(0).toUpperCase() + w.app.slice(1),
            icon: "wildos-windows-app", title: w.title || ""
        }));
    }

    function bytesText(bytes: real): string {
        if (bytes < 1073741824) {
            return Math.max(1, Math.round(bytes / 1048576)) + " MB";
        }
        const gb = bytes / 1073741824;
        return (gb >= 10 ? Math.round(gb) : Math.round(gb * 10) / 10) + " GB";
    }

    function plural(n: int, one: string, many: string): string {
        return n === 1 ? "1 " + one : n + " " + many;
    }

    // QEMU's processor seconds over the time between the first and last
    // sample, as a share of every processor Windows has.
    function share(from: var, to: var): real {
        const seconds = (to.t - from.t) / 1000;
        if (seconds <= 0) {
            return 0;
        }
        return Math.max(0, Math.min(100, (to.cpu - from.cpu) / seconds / processors * 100));
    }

    readonly property int processorPercent: samples.length < 2 ? 0 : Math.round(share(samples[0], samples[samples.length - 1]))

    // Each step between samples as {x: 0 to 1 across the last minute, y: %}.
    readonly property var processorTrace: {
        const out = [];
        const start = now - 60000;
        for (let i = 1; i < samples.length; i++) {
            out.push({ x: Math.max(0, (samples[i].t - start) / 60000), y: share(samples[i - 1], samples[i]) });
        }
        return out;
    }

    function addSample(q: var): void {
        if (!q || typeof q.cpu_seconds !== "number") {
            samples = [];
            return;
        }
        const t = Date.now();
        const last = samples.length > 0 ? samples[samples.length - 1] : null;
        // A lower count means a new QEMU (Windows started again).
        const kept = last && q.cpu_seconds >= last.cpu && q.pid === last.pid ? samples.filter(s => s.t >= t - 63000) : [];
        kept.push({ t: t, cpu: q.cpu_seconds, pid: q.pid });
        samples = kept;
    }

    function editionText(): string {
        const id = String(system && system.edition || "").toLowerCase();
        const names = {
            professional: "Windows 11 Pro", professionalworkstation: "Windows 11 Pro for Workstations",
            core: "Windows 11 Home", coresinglelanguage: "Windows 11 Home", corecountryspecific: "Windows 11 Home",
            enterprise: "Windows 11 Enterprise", education: "Windows 11 Education"
        };
        const name = names[id] || "Windows 11";
        const activated = setupRecord && setupRecord.licence ? setupRecord.licence.activated : null;
        return activated === true ? name + ", activated" : activated === false ? name + ", not activated" : name;
    }

    // When the idle action ran, from the system service's record of it.
    function sinceText(action: string, lead: string): string {
        const last = system ? system.last : null;
        if (!last || last.action !== action || !(last.at > 0)) {
            return "";
        }
        const minutes = Math.floor((now / 1000 - last.at) / 60);
        if (minutes < 1) {
            return lead + " just now";
        }
        if (minutes < 60) {
            return lead + " for " + plural(minutes, "minute", "minutes");
        }
        return lead + " since " + new Date(last.at * 1000).toLocaleTimeString(Qt.locale(), Locale.ShortFormat);
    }

    function subtitle(): string {
        switch (level) {
        case "running":
            return editionText();
        case "paused":
            return sinceText("pause", "Paused") || "Paused";
        case "saved":
            return sinceText("save", "Saved to disk") || "Saved to disk";
        case "off":
            return "Switched off";
        case "starting":
            return "Getting Windows ready";
        case "updating":
            return "Installing updates";
        }
        return "Needs attention";
    }

    // What the panel says the idle policy will do (winmode/lifecycle.py).
    // Saving to disk is off in the service for now, and then it pauses.
    function idleText(): string {
        if (!system) {
            return "";
        }
        const toDisk = canSave && (system.when_idle === "save" || (system.when_idle === "automatic" && system.on_battery));
        const seconds = Number(system.idle_seconds) || 60;
        const wait = seconds === 60 ? "a minute" : seconds % 60 === 0 ? plural(seconds / 60, "minute", "minutes") : plural(seconds, "second", "seconds");
        if (openApps.length > 0) {
            return (toDisk ? "Saves to disk " : "Pauses ") + wait + " after you close the last one.";
        }
        if (typeof system.idle_in === "number") {
            const left = Math.max(1, Math.round(system.idle_in));
            return "No Windows apps are open. " + (toDisk ? "Saves to disk in " : "Pauses in ") + plural(left, "second", "seconds") + ".";
        }
        return "";
    }

    function attentionText(): string {
        if (!answering || !system) {
            return "Windows mode isn't answering. Restart the PC, then try again.";
        }
        if (system.state === "Error") {
            return "Windows stopped unexpectedly. Restart the PC, then open a Windows app to start it again.";
        }
        const last = system.last && system.last.action === "save" ? "save Windows to disk" : "pause Windows";
        return "Windows mode couldn't " + last + ", so it's still using memory. Restarting the PC puts it right.";
    }

    P5Support.DataSource {
        id: reader
        engine: "executable"
        // Each read is its own source (a numbered comment): the engine hands
        // a source it already has its last output instead of running it.
        property string current: ""
        onNewData: (source, data) => {
            disconnectSource(source);
            if (source === current) {
                current = "";
                winmode.receive(String(data.stdout));
            }
        }
    }

    property int readCount: 0
    property int misses: 0

    function refresh(): void {
        if (reader.current !== "") {
            return;
        }
        readCount += 1;
        reader.current = statusCommand + " #" + readCount;
        reader.connectSource(reader.current);
    }

    function receive(stdout: string): void {
        let parsed;
        try {
            parsed = JSON.parse(stdout);
        } catch (e) {
            return;
        }
        if (!parsed || typeof parsed !== "object") {
            return;
        }
        // The services can be slow to answer while the desktop is still
        // starting, so one miss is never shown as a problem.
        if (parsed.answering === false && misses < 1) {
            misses += 1;
            return;
        }
        misses = 0;
        now = Date.now();
        answering = parsed.answering !== false;
        agent = parsed.agent && typeof parsed.agent === "object" ? parsed.agent : null;
        system = parsed.system && typeof parsed.system === "object" ? parsed.system : null;
        addSample(qemu);
        if (!shown) {
            expanded = false;
        }
    }

    // Every second while the panel is open, for the processor graph and the
    // memory figure; every ten seconds behind the chip; once a minute on a PC that hasn't
    // set Windows mode up, which costs a check for a file and nothing more.
    Timer {
        running: true
        repeat: true
        triggeredOnStart: true
        interval: winmode.expanded ? 1000 : winmode.setUp ? 10000 : 60000
        onTriggered: winmode.refresh()
    }

    P5Support.DataSource {
        id: shell
        engine: "executable"
        onNewData: (source, data) => {
            disconnectSource(source);
            winmode.commandFinished(source, String(data.stdout).trim());
        }
    }

    property int runCount: 0

    function run(command: string): void {
        runCount += 1;
        shell.connectSource(command + " #" + runCount);
    }

    function commandFinished(command: string, answer: string): void {
        if (command.indexOf("nproc") === 0) {
            hostCores = parseInt(answer, 10) || 0;
            return;
        }
        refresh();
    }

    function pauseNow(): void {
        run("wildos-winmode idle pause");
    }

    function saveToDisk(): void {
        run("wildos-winmode idle save");
    }

    // The app's Start entry through kstart, the way Task Manager and Start
    // open Windows apps, so the taskbar shows it starting.
    function openApp(id: string): void {
        if (!/^[a-z0-9-]+$/.test(id)) {
            return;
        }
        const entry = "wildos-winmode-" + id;
        run("if command -v kstart >/dev/null 2>&1 && [ -e \"${XDG_DATA_HOME:-$HOME/.local/share}/applications/" + entry + ".desktop\" ]; "
            + "then kstart --application " + entry + "; else wildos-winmode launch " + id + "; fi");
        expanded = false;
    }

    // Settings ships with Wild OS, but if it has gone the click still says
    // something rather than doing nothing.
    function openSettings(): void {
        run("if command -v wildos-settings >/dev/null; then setsid -f wildos-settings --page windowsmode >/dev/null 2>&1; "
            + "else notify-send -a 'Wild OS' -i preferences-system 'Settings is missing' "
            + "'Reinstall it from the Store to change this.'; fi");
        expanded = false;
    }

    onExpandedChanged: () => {
        if (winmode.expanded) {
            now = Date.now();
            refresh();
        }
    }

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.icon: "wildos-windows-app"
    Plasmoid.title: "Windows mode"
    Plasmoid.status: !shown ? PlasmaCore.Types.HiddenStatus
        : level === "attention" ? PlasmaCore.Types.NeedsAttentionStatus : PlasmaCore.Types.PassiveStatus
    toolTipMainText: "Windows mode: " + stateText.charAt(0).toLowerCase() + stateText.slice(1)
    toolTipSubText: level === "attention" ? attentionText()
        : memoryHeld > 0 ? bytesText(memoryHeld) + " of memory held" : "Holding no memory"

    preferredRepresentation: compactRepresentation
    compactRepresentation: Chip {
        winmode: winmode
    }
    fullRepresentation: StatusPanel {
        winmode: winmode
    }

    Component.onCompleted: {
        if (Plasmoid.hasOwnProperty("activationTogglesExpanded")) {
            Plasmoid.activationTogglesExpanded = true;
        }
        run("nproc");
    }
}
