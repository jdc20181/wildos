import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Updates, laid out as Updates.dc.html: the status card, how updates are
// installed, what was installed recently and the way to app updates. System
// updates go through PackageKit, the service the Store uses, so both always
// agree on what is waiting; the automatic ones are apt's unattended
// upgrades, set through a small polkit helper.
PageScroll {
    id: page

    property bool showAll: false
    property string policyError
    property date now: new Date()
    // The restart picked in the Wild OS restart prompt (wildos-dialogs), read
    // again each time the clock above ticks.
    readonly property string restartAt: page.now, System.config("wildos-restartrc", "Restart", "Label", "")
    readonly property string restartPrompt: "/usr/libexec/wildos/wildos-dialogs"

    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    Component.onCompleted: {
        System.refreshUpdatePolicy();
        System.refreshAppUpdates();
    }

    Connections {
        target: System
        function onUpdatePolicyFailed(reason) {
            page.policyError = reason;
            autoToggle.checked = Qt.binding(() => page.policy.auto === true);
        }
        function onUpdatePolicyChanged() {
            page.policyError = "";
        }
    }

    readonly property bool busy: Updates.state === "checking" || Updates.state === "installing"
    readonly property var policy: System.updatePolicy
    readonly property string version: System.osName + (System.osVersion.length > 0 ? " " + System.osVersion : "") + "."
    readonly property string lastChecked: {
        page.now;
        // PackageKit may not have refreshed yet when apt was refreshed some
        // other way; the status tool reads apt's own timestamps.
        const fromApt = parseInt(Protection.status.updates_checked || "0");
        const when = Format.isValid(Updates.lastChecked) ? Format.ago(Updates.lastChecked)
            : fromApt > 0 ? Format.ago(new Date(fromApt * 1000)) : "";
        return when.length > 0 ? "Last checked " + when + "." : "";
    }

    // Updates are shown by what they are to a person, never by package:
    // apps by their own names, everything else by the part of the system it
    // belongs to, as Windows Update words its entries.
    readonly property var groupNames: ({
        "wildos": "Wild OS", "core": "Core system", "browser": "Web browser", "desktop": "Desktop and graphics",
        "library": "System libraries", "other": "System tools"
    })

    function groupTitle(group, security) {
        switch (group) {
        case "wildos": return "Wild OS update";
        case "core": return security ? "Core system security fixes" : "Core system updates";
        case "browser": return security ? "Security fixes for the browser" : "Browser update";
        case "desktop": return security ? "Desktop security fixes" : "Desktop updates";
        case "library": return security ? "Security fixes for system libraries" : "System library updates";
        default: return security ? "Security fixes for system tools" : "System tool updates";
        }
    }

    // Packages gathered into the groups a person reads, security first.
    function gather(packages, security) {
        const groups = [];
        const byKey = {};
        packages.forEach((name, i) => {
            const d = Updates.describe(name);
            const key = d.group === "app" ? "app:" + d.title : d.group;
            let g = byKey[key];
            if (!g) {
                g = { group: d.group, app: d.title, count: 0, security: false };
                byKey[key] = g;
                groups.push(g);
            }
            g.count += 1;
            g.security = g.security || (security ? security[i] === true : false);
        });
        return groups.sort((a, b) => (b.security ? 1 : 0) - (a.security ? 1 : 0));
    }

    function label(g) {
        return g.group === "app" ? g.app : groupNames[g.group];
    }

    function listOf(names) {
        if (names.length <= 1) {
            return names.join("");
        }
        const shown = names.slice(0, 3);
        const more = names.length - shown.length;
        return more > 0 ? shown.join(", ") + " and " + more + " more" : shown.slice(0, -1).join(", ") + " and " + shown[shown.length - 1];
    }

    readonly property var pending: gather(Updates.updates.map(u => u.name), Updates.updates.map(u => u.security))

    function historyTitle(entry) {
        const groups = gather(entry.packages);
        const apps = groups.filter(g => g.group === "app").map(g => g.app);
        if (entry.kind === "update") {
            if (!entry.succeeded) {
                return "Updates that did not finish";
            }
            if (groups.some(g => g.group === "wildos")) {
                return "Wild OS update";
            }
            if (groups.length === 1) {
                return groups[0].group === "app" ? "Update to " + groups[0].app : groupTitle(groups[0].group, false);
            }
            return Format.plural(entry.packages.length, "update", "updates");
        }
        const verb = entry.kind === "install" ? "Installed " : "Removed ";
        if (!entry.succeeded) {
            return entry.kind === "install" ? "An install that did not finish" : "A removal that did not finish";
        }
        return verb + (apps.length > 0 ? listOf(apps) : Format.plural(entry.packages.length, "system part", "system parts"));
    }

    function historyDetail(entry) {
        page.now;
        const when = Format.day(entry.when);
        const verb = entry.kind === "remove" ? "Removed " : "Installed ";
        const prefix = when.length > 0 ? verb + when : verb.trim();
        const groups = gather(entry.packages);
        return groups.length > 1 ? prefix + ": " + listOf(groups.map(label)) : prefix;
    }

    // Only the newest Wild OS update can show its notes: the changelog on
    // disk is the one that update installed.
    readonly property string releaseNotes: Updates.releaseNotes()
    readonly property int newestRelease: Updates.history.findIndex(e => e.kind === "update" && e.succeeded && e.packages.some(n => n.startsWith("wildos")))

    StatusCard {
        iconCircle: 60
        iconSize: 30
        iconStroke: Updates.state === "uptodate" || Updates.state === "installed" ? 2.6 : 2.2
        spinning: Updates.state === "checking"
        warning: Updates.state === "error" || (Updates.state === "available" && Updates.securityCount > 0)
        iconName: Updates.state === "error" ? "warning"
                : Updates.state === "uptodate" || Updates.state === "installed" ? "check"
                : "update"
        title: {
            switch (Updates.state) {
            case "checking": return "Checking for updates";
            case "installing": return "Installing updates";
            case "available":
                return Updates.securityCount > 0
                    ? Format.plural(Updates.securityCount, "security fix is", "security fixes are") + " ready to install"
                    : Format.plural(Updates.updates.length, "update is", "updates are") + " ready to install";
            case "installed": return Updates.restartNeeded ? "Restart to finish updating" : "Updates installed";
            case "uptodate": return "You're up to date";
            case "error": return Updates.updates.length > 0 ? "The updates were not installed" : "Wild OS could not check for updates";
            default: return Updates.available ? "Updates" : "Updates are not available";
            }
        }
        description: {
            switch (Updates.state) {
            case "checking": return "This can take a minute on a slow connection.";
            case "installing":
                return (Updates.progress > 0 ? Updates.progress + "% done." : "Getting ready.")
                    + (Updates.detail.length > 0 ? " Now: " + Updates.detail + "." : "");
            case "available":
                return (Updates.securityCount > 0 ? "Install them now to stay protected. " : "") + page.lastChecked;
            case "installed":
                return Updates.restartNeeded
                    ? (page.restartAt.length > 0 ? "Wild OS will restart " + page.restartAt.charAt(0).toLowerCase() + page.restartAt.slice(1) + ", as you chose." : "Some updates only take effect after a restart.")
                    : page.version + " " + page.lastChecked;
            case "error": return Updates.error;
            case "uptodate": return page.version + " " + page.lastChecked;
            default:
                return Updates.available ? page.version + " " + page.lastChecked : "The update service is not installed on this computer.";
            }
        }

        UI.Button {
            visible: Updates.restartNeeded && !page.busy
            text: "Restart now"
            kind: "primary"
            onClicked: Updates.restart()
        }

        UI.Button {
            visible: Updates.restartNeeded && !page.busy && System.hasProgram(page.restartPrompt)
            text: page.restartAt.length > 0 ? "Change time" : "Schedule restart"
            onClicked: System.launch(page.restartPrompt, ["restart"])
        }

        UI.Button {
            visible: Updates.state === "available"
            text: "Install now"
            kind: !Updates.restartNeeded ? "primary" : "secondary"
            onClicked: Updates.install()
        }

        UI.Button {
            visible: Updates.state !== "available" && Updates.state !== "installing"
            enabled: Updates.available && !page.busy
            text: Updates.state === "checking" ? "Checking" : "Check now"
            onClicked: Updates.check()
        }
    }

    UI.ProgressBar {
        Layout.fillWidth: true
        visible: Updates.state === "installing"
        barHeight: 8
        value: Updates.progress / 100
        indeterminate: Updates.progress <= 0
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: Updates.updates.length > 0
        spacing: 10

        SectionTitle {
            text: "Ready to install"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: page.showAll ? page.pending : page.pending.slice(0, 6)

                    UI.SettingRow {
                        required property var modelData
                        title: modelData.group === "app" ? modelData.app : page.groupTitle(modelData.group, modelData.security)
                        description: modelData.group === "app" ? "App update" : Format.plural(modelData.count, "update", "updates")
                        status: modelData.security ? "Security fix" : ""
                    }
                }

                UI.SettingRow {
                    visible: !page.showAll && page.pending.length > 6
                    title: "Show all " + page.pending.length + " groups of updates"
                    chevron: true
                    onClicked: page.showAll = true
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                title: "Install security updates automatically"
                description: page.policyError.length > 0 ? page.policyError
                    : page.policy.available === false ? "The automatic updates service is not installed on this computer."
                    : "Small fixes install quietly in the background."
                enabled: page.policy.available === true
                status: (page.policy.auto === true) ? "On" : "Off"

                UI.Toggle {
                    id: autoToggle
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.policy.auto === true
                    Accessible.name: "Install security updates automatically"
                    onToggled: System.setUpdatePolicy("auto", checked ? "on" : "off")
                }
            }

            UI.SettingRow {
                title: "When a restart is needed"
                description: "Wild OS will never restart in the middle of your work"

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "When a restart is needed"
                    enabled: page.policy.available === true
                    model: [
                        { value: "ask", text: "Ask me first" },
                        { value: "idle", text: "Restart when no one is signed in" }
                    ]
                    readonly property var choice: page.policy.restart || "ask"
                    currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                    onActivated: index => System.setUpdatePolicy("restart", model[index].value)
                }
            }

            // Beta gets each new build as soon as it is made, before it is
            // promoted to Stable.
            UI.SettingRow {
                title: "Update channel"
                description: "Stable gets tested releases. Recommended for most people."

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Update channel"
                    enabled: page.policy.channelAvailable === true
                    model: [
                        { value: "stable", text: "Stable" },
                        { value: "beta", text: "Beta" }
                    ]
                    readonly property var choice: page.policy.channel || "stable"
                    currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                    onActivated: index => System.setUpdatePolicy("channel", model[index].value)
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Recently installed"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: Updates.history.slice(0, 5)

                    UI.SettingRow {
                        required property var modelData
                        required property int index
                        title: page.historyTitle(modelData)
                        description: page.historyDetail(modelData)

                        UI.LinkButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: index === page.newestRelease && page.releaseNotes.length > 0
                            text: "What's new"
                            onClicked: notesDialog.open()
                        }
                    }
                }

                UI.SettingRow {
                    visible: Updates.history.length === 0
                    title: "Nothing installed yet"
                    description: "Updates appear here once they are installed."
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: System.storeApp.length > 0

        Column {
            width: parent.width

            UI.SettingRow {
                title: "App updates"
                description: System.appUpdates > 0
                    ? Format.plural(System.appUpdates, "app has an update", "apps have updates") + " waiting in the Store"
                    : System.appUpdates === 0 ? "Apps from the Store are up to date"
                    : "Apps you installed from the Store update there"
                chevron: true
                onClicked: System.openStore("updates")
            }
        }
    }

    UI.Dialog {
        id: notesDialog
        title: "What's new in " + page.version.replace(/\.$/, "")

        UI.Label {
            Layout.fillWidth: true
            text: page.releaseNotes.split("\n").map(line => "•  " + line).join("\n")
            color: UI.Style.inkStatus
            lineHeight: 1.4
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        buttons: [
            UI.Button {
                text: "Close"
                kind: "primary"
                onClicked: notesDialog.close()
            }
        ]
    }
}
