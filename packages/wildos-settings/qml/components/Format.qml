pragma Singleton

import QtQuick
import WildOS.UI as UI

// Times and counts in the house style: day-first dates, plain words for
// recent times, and times in the regional format on the clock chosen on
// the Date and time page.
QtObject {
    function pad(n) {
        return n < 10 ? "0" + n : "" + n;
    }

    function time(date) {
        return Qt.locale().toString(date, UI.TimeFormat.shortFormat);
    }

    // Whether times carry AM or PM, from the clock chosen or the regional
    // format's own.
    function twelveHour() {
        return /a/i.test(UI.TimeFormat.shortFormat.replace(/'[^']*'/g, ""));
    }

    readonly property var days: ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
    readonly property var months: ["January", "February", "March", "April", "May", "June", "July", "August",
        "September", "October", "November", "December"]

    function longDate(date) {
        return days[date.getDay()] + " " + date.getDate() + " " + months[date.getMonth()];
    }

    function shortDate(date) {
        return pad(date.getDate()) + "/" + pad(date.getMonth() + 1) + "/" + date.getFullYear();
    }

    function plural(n, one, many) {
        return n === 1 ? "1 " + one : n + " " + many;
    }

    // "today", "yesterday", "Tuesday" within the week, then "21 October".
    function day(date) {
        if (!isValid(date)) {
            return "";
        }
        const now = new Date();
        const midnight = new Date(now.getFullYear(), now.getMonth(), now.getDate());
        const then = new Date(date.getFullYear(), date.getMonth(), date.getDate());
        const diff = Math.round((midnight - then) / 86400000);
        if (diff <= 0) {
            return "today";
        }
        if (diff === 1) {
            return "yesterday";
        }
        if (diff < 7) {
            return days[date.getDay()];
        }
        return date.getDate() + " " + months[date.getMonth()] + (date.getFullYear() === now.getFullYear() ? "" : " " + date.getFullYear());
    }

    function isValid(date) {
        return date instanceof Date && !isNaN(date.getTime());
    }

    // "just now", "10 minutes ago", "today at 13:10", "06/11/2026 at 13:10"
    function ago(date) {
        if (!isValid(date)) {
            return "";
        }
        const now = new Date();
        const minutes = Math.floor((now - date) / 60000);
        if (minutes < 1) {
            return "just now";
        }
        if (minutes < 60) {
            return plural(minutes, "minute", "minutes") + " ago";
        }
        const sameDay = now.toDateString() === date.toDateString();
        if (sameDay) {
            return "today at " + time(date);
        }
        const yesterday = new Date(now.getTime() - 86400000);
        if (yesterday.toDateString() === date.toDateString()) {
            return "yesterday at " + time(date);
        }
        return shortDate(date) + " at " + time(date);
    }
}
