pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as P5Support
import org.kde.notificationmanager as NotificationManager

// The clock at the end of the taskbar: 24-hour time over the date, opening
// the design's notifications and calendar cards. With "showPopups" on it also
// shows new notifications as the design's cards in the corner of the screen,
// in place of Plasma's own notifications widget.
PlasmoidItem {
    id: clock

    // Plasma's time engine follows time zone changes, which a plain Date in
    // this process would not notice until plasmashell restarts.
    P5Support.DataSource {
        id: timeSource
        engine: "time"
        connectedSources: ["Local"]
        interval: 60000
        intervalAlignment: P5Support.Types.AlignToMinute
    }

    readonly property date now: timeSource.data.Local ? timeSource.data.Local.DateTime : new Date()

    NotificationManager.Settings {
        id: settings
        live: true
    }

    readonly property bool doNotDisturb: {
        const until = settings.notificationsInhibitedUntil;
        const timed = !isNaN(until.getTime()) && now.getTime() < until.getTime();
        return timed || settings.notificationsInhibitedByApplication
            || (settings.inhibitNotificationsWhenScreensMirrored && settings.screensMirrored);
    }

    readonly property alias history: historyModel

    // As in Plasma and quick settings, a year from now stands for "until
    // switched off"; switching off also lifts any app's own inhibition.
    function setDoNotDisturb(on: bool): void {
        if (on) {
            const until = new Date();
            until.setFullYear(until.getFullYear() + 1);
            settings.notificationsInhibitedUntil = until;
        } else {
            settings.notificationsInhibitedUntil = undefined;
            settings.revokeApplicationInhibitions();
        }
        settings.save();
    }

    P5Support.DataSource {
        id: shell
        engine: "executable"
        onNewData: source => disconnectSource(source)
    }

    // Settings ships with Wild OS, but if it has gone the click still says
    // something rather than doing nothing.
    function openSettings(page: string): void {
        shell.connectSource("if command -v wildos-settings >/dev/null; then setsid -f wildos-settings --page " + page + " >/dev/null 2>&1; "
            + "else notify-send -a 'Wild OS' -i preferences-system 'Settings is missing' "
            + "'Reinstall it from the Store to change this.'; fi");
        clock.expanded = false;
    }

    NotificationManager.Notifications {
        id: historyModel
        showExpired: true
        showDismissed: true
        showJobs: settings.jobsInNotifications
        sortMode: NotificationManager.Notifications.SortByDate
        sortOrder: Qt.DescendingOrder
        groupMode: NotificationManager.Notifications.GroupDisabled
        blacklistedDesktopEntries: settings.historyBlacklistedApplications
        blacklistedNotifyRcNames: settings.historyBlacklistedServices
        urgencies: NotificationManager.Notifications.CriticalUrgency
            | NotificationManager.Notifications.NormalUrgency
            | (settings.lowPriorityHistory ? NotificationManager.Notifications.LowUrgency : 0)
    }

    readonly property bool showPopups: Plasmoid.configuration.showPopups

    // Plasma's notifications widget normally reports do not disturb to apps.
    // When this clock shows the pop-ups instead, it does that job too.
    Binding {
        when: clock.showPopups
        target: NotificationManager.Server
        property: "inhibited"
        value: clock.doNotDisturb
        restoreMode: Binding.RestoreBinding
    }

    Loader {
        active: clock.showPopups
        sourceComponent: Toasts {
            clock: clock
            settings: settings
        }
    }

    // The Caps Lock and Num Lock square in the middle of this screen. Plasma
    // has no indicator of its own for these, so the clock, which is always
    // on the taskbar, carries it.
    Loader {
        active: Plasmoid.formFactor === PlasmaCore.Types.Horizontal || Plasmoid.formFactor === PlasmaCore.Types.Vertical
        sourceComponent: LockKeys {
            screenInfo: {
                const screens = Qt.application.screens;
                if (Plasmoid.containment) {
                    const g = Plasmoid.containment.screenGeometry;
                    for (let i = 0; i < screens.length; i++) {
                        if (screens[i].virtualX === g.x && screens[i].virtualY === g.y) {
                            return screens[i];
                        }
                    }
                }
                return screens.length > 0 ? screens[0] : null;
            }
        }
    }

    // These take the model the notification came from and its index there.
    function closeNotification(model: var, index: var): void {
        model.close(index);
    }

    function invokeDefault(model: var, index: var, resident: bool): void {
        model.invokeDefaultAction(index,
            resident ? NotificationManager.Notifications.None : NotificationManager.Notifications.Close);
    }

    function invokeAction(model: var, index: var, name: string, resident: bool): void {
        model.invokeAction(index, name,
            resident ? NotificationManager.Notifications.None : NotificationManager.Notifications.Close);
    }

    // "Clear all" empties the list, as Windows does: finished notifications
    // are removed and any still showing are dismissed. Running jobs stay.
    function clearAll(): void {
        history.clear(NotificationManager.Notifications.ClearExpired);
        for (let row = history.count - 1; row >= 0; row--) {
            const index = history.index(row, 0);
            if (history.data(index, NotificationManager.Notifications.TypeRole) !== NotificationManager.Notifications.JobType) {
                history.close(index);
            }
        }
    }

    onExpandedChanged: {
        if (!clock.expanded) {
            history.lastRead = new Date();
        }
    }

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.icon: "preferences-system-time"
    Plasmoid.title: "Date, time and notifications"
    toolTipMainText: Qt.locale("en_GB").toString(now, "dddd d MMMM yyyy")
    toolTipSubText: {
        const unread = history.unreadNotificationsCount;
        const lines = [];
        if (unread > 0) {
            lines.push(unread === 1 ? "1 new notification" : unread + " new notifications");
        }
        if (doNotDisturb) {
            lines.push("Do not disturb is on");
        }
        return lines.join("\n");
    }

    preferredRepresentation: compactRepresentation
    compactRepresentation: ClockButton {
        clock: clock
    }
    fullRepresentation: ClockFlyout {
        clock: clock
    }

    Component.onCompleted: {
        if (Plasmoid.hasOwnProperty("activationTogglesExpanded")) {
            Plasmoid.activationTogglesExpanded = true;
        }
    }
}
