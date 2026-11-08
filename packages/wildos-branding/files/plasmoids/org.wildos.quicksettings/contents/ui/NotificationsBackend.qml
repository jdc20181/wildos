import QtQuick
import org.kde.notificationmanager as NotificationManager

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

    // A timed do not disturb ends on its own, so check once a minute.
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: dnd.now = Date.now()
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
