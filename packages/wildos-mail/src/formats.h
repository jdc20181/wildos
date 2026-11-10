#pragma once

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>

// Dates and times in the regional format chosen in Settings (LC_TIME, which
// QLocale::system() follows), with day and month names in English as the
// rest of Wild OS is. qml/Formats.js does the same for the QML side.
namespace Formats
{
inline QLocale names()
{
    return QLocale(QLocale::English, QLocale::UnitedKingdom);
}

inline QString time(const QDateTime &when)
{
    return QLocale::system().toString(when.time(), QLocale::ShortFormat);
}

inline QString shortDate(const QDate &day)
{
    return names().toString(day, QLocale::system().dateFormat(QLocale::ShortFormat));
}

// "Mon 9 Nov", "Mon, Nov 9": the long date without its year, with short names.
inline QString shortDayAndMonth(const QDate &day)
{
    QString format = QLocale::system().dateFormat(QLocale::LongFormat);
    format.remove(QRegularExpression(QStringLiteral("[,.\s]*y+[.\u5e74]?")));
    format.replace(QStringLiteral("dddd, d MMMM"), QStringLiteral("dddd d MMMM"));
    format.replace(QStringLiteral("dddd"), QStringLiteral("ddd"));
    format.replace(QStringLiteral("MMMM"), QStringLiteral("MMM"));
    return names().toString(day, format.trimmed());
}
}
