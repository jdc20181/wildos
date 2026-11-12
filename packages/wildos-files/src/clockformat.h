#pragma once

// The clock choice from Settings > Time and language > Date and time: the
// regional format's own clock (the default), or 12-hour or 24-hour whatever
// the regional format. Settings writes it to wildosrc [Clock] Hours as 12,
// 24, or 0 for the regional format.
//
// WildOS.UI's TimeFormat singleton uses this for every QML clock. Wild OS's
// C++ apps cannot link a QML plugin, so each one that writes times carries
// a copy of this file. packages/wildos-ui/src/clockformat.h is the one to
// edit; tools/check-deps.sh fails if a copy differs from it.

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QString>

namespace ClockFormat
{

// Whether a Qt time format has an AM/PM marker outside quoted text, which
// is what puts Qt's h on the 12-hour clock.
inline bool isTwelveHour(const QString &format)
{
    bool quoted = false;
    for (const QChar c : format) {
        if (c == QLatin1Char('\'')) {
            quoted = !quoted;
        } else if (!quoted && (c == QLatin1Char('a') || c == QLatin1Char('A'))) {
            return true;
        }
    }
    return false;
}

// Rewrites a Qt time format for the 12-hour clock (hours 12) or the 24-hour
// clock (hours 24); anything else, or a format already on that clock, comes
// back as it is, so en_AU keeps its "pm" and Japanese its "9:05". Only the
// hour and the AM/PM marker change, so separators, quoted text and the
// locale's order stay as they are.
inline QString rewrite(const QString &format, int hours)
{
    const bool twelve = isTwelveHour(format);
    if ((hours != 12 && hours != 24) || (hours == 12) == twelve) {
        return format;
    }
    QString out;
    bool quoted = false;
    qsizetype dropped = -1;
    for (qsizetype i = 0; i < format.size(); ++i) {
        const QChar c = format.at(i);
        if (c == QLatin1Char('\'')) {
            quoted = !quoted;
        }
        if (quoted || c == QLatin1Char('\'')) {
            out += c;
            continue;
        }
        if (c == QLatin1Char('h') || c == QLatin1Char('H')) {
            while (i + 1 < format.size() && format.at(i + 1).toLower() == QLatin1Char('h')) {
                ++i;
            }
            // 13:30 and 09:30 on the 24-hour clock, 1:30 PM on the 12-hour.
            out += hours == 24 ? QStringLiteral("HH") : QStringLiteral("h");
            continue;
        }
        if (c == QLatin1Char('a') || c == QLatin1Char('A')) {
            if (i + 1 < format.size() && format.at(i + 1).toLower() == QLatin1Char('p')) {
                ++i;
            }
            dropped = out.size();
            continue;
        }
        out += c;
    }
    if (dropped >= 0) {
        // The space that stood between the marker and the time goes too, on
        // whichever side it was: "h:mm AP" and "AP h:mm" both give "HH:mm".
        if (dropped > 0 && out.at(dropped - 1).isSpace()) {
            out.remove(dropped - 1, 1);
        } else if (dropped < out.size() && out.at(dropped).isSpace()) {
            out.remove(dropped, 1);
        }
    }
    if (hours == 12) {
        out += QStringLiteral(" AP");
    }
    return out;
}

// The short format with seconds after the minutes, with the locale's own
// separator; Qt's long format has seconds but adds the time zone.
inline QString withSeconds(const QString &format)
{
    const qsizetype at = format.indexOf(QStringLiteral("mm"));
    if (at < 0 || format.contains(QLatin1Char('s'))) {
        return format;
    }
    const QChar before = at > 0 ? format.at(at - 1) : QChar();
    const QChar separator = before.isPunct() && before != QLatin1Char('\'') ? before : QLatin1Char(':');
    return QString(format).insert(at + 2, separator + QStringLiteral("ss"));
}

// A time written in a format with its AM/PM marker taken out, for clocks
// drawn too large to carry it, such as the lock screen's. Qt has no format
// for that (h without a marker is the 24-hour hour), so the marker's text
// is removed from the result.
inline QString withoutMarker(const QLocale &locale, const QTime &when, const QString &format)
{
    QString text = locale.toString(when, format);
    if (!isTwelveHour(format)) {
        return text;
    }
    for (const QString &marker : {QStringLiteral("Ap"), QStringLiteral("AP"), QStringLiteral("ap")}) {
        text.remove(locale.toString(when, marker));
    }
    return text.simplified();
}

// 12, 24, or 0 for the regional format's own clock. The file is read again
// only when it changes, so this is cheap enough to call for every row.
inline int hours()
{
    static const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/wildosrc");
    static bool loaded = false;
    static QDateTime seen;
    static int cached = 0;
    const QFileInfo info(path);
    const QDateTime modified = info.exists() ? info.lastModified() : QDateTime();
    if (loaded && modified == seen) {
        return cached;
    }
    loaded = true;
    seen = modified;
    cached = 0;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return cached;
    }
    bool inGroup = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGroup = line == QLatin1String("[Clock]");
        } else if (inGroup && line.section(QLatin1Char('='), 0, 0).trimmed() == QLatin1String("Hours")) {
            const int value = line.section(QLatin1Char('='), 1).trimmed().toInt();
            cached = value == 12 || value == 24 ? value : 0;
        }
    }
    return cached;
}

// The time format to use: the regional format (LC_TIME, which
// QLocale::system() follows) with the clock choice applied.
inline QString timeFormat(bool seconds = false)
{
    const QString format = rewrite(QLocale::system().timeFormat(QLocale::ShortFormat), hours());
    return seconds ? withSeconds(format) : format;
}

inline QString time(const QTime &when, bool seconds = false)
{
    return QLocale::system().toString(when, timeFormat(seconds));
}

}
