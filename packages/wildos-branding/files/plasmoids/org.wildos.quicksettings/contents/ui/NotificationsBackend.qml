import QtQuick
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.plasma5support as P5Support

// Do not disturb is the notification server's inhibition setting. The clock
// flyout and Plasma read the same setting, so all of them agree.
Item {
    id: dnd

    readonly property bool active: {
        const until = settings.notificationsInhibitedUntil;
        const timed = !isNaN(until.getTime()) && now < until.getTime();
        return timed || settings.notificationsInhibitedByApplication;
    }
    property real now: Date.now()

    NotificationManager.Settings {
        id: settings
        live: true
    }

    // A timed do not disturb ends on its own, so check once a minute. The
    // same tick checks quiet hours.
    Timer {
        interval: 60000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            dnd.now = Date.now();
            quietHours.connectSource(dnd.readQuietHours);
        }
    }

    // Quiet hours, set in Settings > System > Notifications and kept in
    // wildosrc: Enabled, From and To as "HHMM".
    readonly property string readQuietHours: "for k in Enabled From To; do kreadconfig6 --file wildosrc --group QuietHours --key $k; done"
    // The start of the quiet period already applied, so turning do not
    // disturb off early is not undone a minute later.
    property real quietApplied: 0

    P5Support.DataSource {
        id: quietHours
        engine: "executable"
        onNewData: (source, data) => {
            disconnectSource(source);
            const lines = String(data.stdout).split("
");
            if (lines[0].trim() !== "true") {
                return;
            }
            dnd.applyQuietHours(lines[1].trim() || "2200", lines[2].trim() || "0700");
        }
    }

    function at(day: date, hhmm: string): date {
        const d = new Date(day);
        d.setHours(Number(hhmm.slice(0, 2)), Number(hhmm.slice(2, 4)), 0, 0);
        return d;
    }

    function applyQuietHours(from: string, to: string): void {
        const now = new Date();
        let start = at(now, from);
        let end = at(now, to);
        if (end <= start) {
            // Overnight: before the end time this morning, the period
            // started yesterday evening.
            if (now < end) {
                start.setDate(start.getDate() - 1);
            } else {
                end.setDate(end.getDate() + 1);
            }
        }
        if (now < start || now >= end || start.getTime() === quietApplied) {
            return;
        }
        quietApplied = start.getTime();
        const until = settings.notificationsInhibitedUntil;
        if (!isNaN(until.getTime()) && until.getTime() >= end.getTime()) {
            return;
        }
        settings.notificationsInhibitedUntil = end;
        settings.save();
        dnd.now = Date.now();
    }

    function setActive(on: bool): void {
        if (on) {
            // As in Plasma, a year from now stands for "until switched off".
            const until = new Date();
            until.setFullYear(until.getFullYear() + 1);
            settings.notificationsInhibitedUntil = until;
        } else {
            settings.notificationsInhibitedUntil = undefined;
            settings.revokeApplicationInhibitions();
        }
        settings.save();
        now = Date.now();
    }
}
