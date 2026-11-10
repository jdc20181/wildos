// Dates and times in the regional format chosen in Settings > Language and
// region (LC_TIME, which Qt.locale() follows), with day and month names in
// English because the rest of Wild OS is in English. The taskbar clock's
// Formats.js works the same way.

function names() {
    return Qt.locale("en_GB");
}

function time(when) {
    return Qt.locale().toString(when, Qt.locale().timeFormat(Locale.ShortFormat));
}

// "Mon 09/11/2026", "Mon 11/9/26".
function dayAndShortDate(when) {
    return names().toString(when, "ddd " + Qt.locale().dateFormat(Locale.ShortFormat));
}

// Qt writes UK dates as "Monday, 9 November 2026"; the design has no comma.
function longFormat() {
    return Qt.locale().dateFormat(Locale.LongFormat).replace(/^dddd, d MMMM/, "dddd d MMMM");
}

function withoutYear(format) {
    return format.replace(/[,.\s]*y+[.年]?/, "").trim();
}

function withoutWeekday(format) {
    return format.replace(/d{4}[,.\s]*/, "").trim();
}

// "Monday 9 November 2026", "Monday, November 9, 2026".
function longDate(when) {
    return names().toString(when, longFormat());
}

// "Mon 9 November 2026", "Mon, November 9, 2026".
function mediumDate(when) {
    return names().toString(when, longFormat().replace("dddd", "ddd"));
}

// "Monday 9 November", "Monday, November 9".
function weekdayAndDay(when) {
    return names().toString(when, withoutYear(longFormat()));
}

// "9 November 2026", "November 9, 2026".
function dayMonthYear(when) {
    return names().toString(when, withoutWeekday(longFormat()));
}

// "9 November", "November 9".
function dayAndMonth(when) {
    return names().toString(when, withoutWeekday(withoutYear(longFormat())));
}

// "9 Nov", "Nov 9".
function shortDayAndMonth(when) {
    return names().toString(when, withoutWeekday(withoutYear(longFormat())).replace("MMMM", "MMM"));
}

// "November 2026" in most formats, "2026年11月" where the year comes first.
function monthAndYear(when) {
    return names().toString(when, withoutWeekday(longFormat()).replace(/d{1,2}[,.\s日]*/, "").trim());
}

// True when the day comes before the month, as in "9 November".
function dayFirst() {
    const f = withoutWeekday(longFormat());
    return f.indexOf("d") < f.indexOf("M");
}
