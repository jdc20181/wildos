// Dates and times in the regional format chosen in Settings > Language and
// region (LC_TIME, which Qt.locale() follows), with day and month names in
// English because the rest of Wild OS is in English.

function names() {
    return Qt.locale("en_GB");
}

function time(when, seconds) {
    let format = Qt.locale().timeFormat(Locale.ShortFormat);
    // The short format has no seconds; the long one adds a time zone, which
    // the taskbar has no room for, so seconds go in after the minutes.
    if (seconds && format.indexOf("s") < 0) {
        format = format.replace(/mm/, "mm:ss");
    }
    return Qt.locale().toString(when, format);
}

function shortDate(when) {
    return names().toString(when, Qt.locale().dateFormat(Locale.ShortFormat));
}

// Qt writes UK dates as "Monday, 9 November"; the design has no comma.
function longFormat() {
    return Qt.locale().dateFormat(Locale.LongFormat).replace(/^dddd, d MMMM/, "dddd d MMMM");
}

function longDate(when) {
    return names().toString(when, longFormat());
}

// The long date without its year: "Monday 9 November", "Monday, November 9".
function dayAndMonth(when) {
    return names().toString(when, longFormat().replace(/[,.\s]*y+[.年]?/, "").trim());
}

// 0 for Sunday to 6 for Saturday, as Date.getDay() numbers them.
function firstDayOfWeek() {
    return Qt.locale().firstDayOfWeek;
}
