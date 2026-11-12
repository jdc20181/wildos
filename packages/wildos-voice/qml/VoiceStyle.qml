pragma Singleton

import QtQuick
import WildOS.UI as UI

// The few values the board gives Voice that the system tokens do not have.
// Red means recording and nothing else: the dot, the bars, Stop and the row
// of the recording in progress (AppVoice.dc #ff5468, AppVoiceLight.dc).
QtObject {
    readonly property color record: "#ff5468"
    readonly property color recordRing: UI.Style.dark ? Qt.rgba(1, 84 / 255, 104 / 255, 0.25) : Qt.rgba(153 / 255, 50 / 255, 62 / 255, 0.175)
    readonly property color recordHalo: UI.Style.dark ? Qt.rgba(1, 84 / 255, 104 / 255, 0.18) : Qt.rgba(153 / 255, 50 / 255, 62 / 255, 0.126)
    readonly property color recordText: UI.Style.contrast ? "#ff8a8a" : UI.Style.dark ? "#ff8a8a" : "#8c4b4b"
    readonly property color recordIcon: UI.Style.dark ? "#ff5468" : "#9e3440"
    // The shield on "Stays on this PC".
    readonly property color safe: UI.Style.dark ? "#5fd38d" : "#3a8257"
    // The list column sits on a neutral shade of the window, as in Music.
    readonly property color sideShade: UI.Style.contrast ? "#000000" : Qt.rgba(0, 0, 0, UI.Style.dark ? 0.12 : 0.06)
    // The round Mark and Pause buttons, and the icons in the list.
    readonly property color roundButton: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.056)
    readonly property color roundButtonHover: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color rowIcon: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.042)
    // The waveform: the dotted line still to come, the part of a recording
    // not played yet, and the playhead.
    readonly property color waveDot: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.13)
    readonly property color waveRest: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.26) : Qt.rgba(0, 0, 0, 0.2)
    readonly property color playhead: UI.Style.dark ? "#ffffff" : UI.Style.ink

    // "02:14", or "1:02:14" past the hour, as the board writes times.
    function time(ms) {
        const total = Math.max(0, Math.floor((ms || 0) / 1000));
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;
        const mmss = (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
        return h > 0 ? h + ":" + mmss : mmss;
    }

    function startOfDay(date) {
        return new Date(date.getFullYear(), date.getMonth(), date.getDate());
    }

    // The list's groups: Today, This week (the six days before) and Earlier.
    function group(created) {
        const today = startOfDay(new Date());
        const day = startOfDay(created);
        const days = Math.round((today - day) / 86400000);
        if (days <= 0) {
            return qsTr("Today");
        }
        return days < 7 ? qsTr("This week") : qsTr("Earlier");
    }

    // Under a title: the time today, the day this week, the date before.
    function when(created) {
        const today = startOfDay(new Date());
        const days = Math.round((today - startOfDay(created)) / 86400000);
        if (days <= 0) {
            return created.toLocaleTimeString(Qt.locale(), "HH:mm");
        }
        if (days < 7) {
            return created.toLocaleDateString(Qt.locale(), "ddd");
        }
        if (created.getFullYear() === today.getFullYear()) {
            return created.toLocaleDateString(Qt.locale(), "d MMM");
        }
        return created.toLocaleDateString(Qt.locale(), "d MMM yyyy");
    }

    // The header of a finished recording: "Today, 10:02", "Thursday, 14:30",
    // "28 October 2026, 09:15".
    function fullDate(created) {
        const today = startOfDay(new Date());
        const days = Math.round((today - startOfDay(created)) / 86400000);
        const clock = created.toLocaleTimeString(Qt.locale(), "HH:mm");
        if (days <= 0) {
            return qsTr("Today, %1").arg(clock);
        }
        if (days === 1) {
            return qsTr("Yesterday, %1").arg(clock);
        }
        if (days < 7) {
            return created.toLocaleDateString(Qt.locale(), "dddd") + ", " + clock;
        }
        return created.toLocaleDateString(Qt.locale(), "d MMMM yyyy") + ", " + clock;
    }
}
