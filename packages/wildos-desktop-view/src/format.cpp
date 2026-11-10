#include "format.h"

#include <QHash>
#include <QLocale>
#include <QMimeDatabase>

namespace Format
{

QString visibleName(const QString &fileName, bool isDir, bool showExtensions)
{
    if (isDir || showExtensions) {
        return fileName;
    }
    // Only suffixes the system knows a type for are hidden, so a file with
    // an unknown or misleading ending still shows it, as Windows does.
    static QMimeDatabase db;
    const QString suffix = db.suffixForFileName(fileName);
    if (suffix.isEmpty() || fileName.size() <= suffix.size() + 1) {
        return fileName;
    }
    return fileName.left(fileName.size() - suffix.size() - 1);
}

QString typeName(const QMimeType &mime, bool isDir)
{
    if (isDir) {
        return QStringLiteral("Folder");
    }
    static const QHash<QString, QString> plain = {
        {QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"), QStringLiteral("Spreadsheet")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"), QStringLiteral("Spreadsheet")},
        {QStringLiteral("application/vnd.ms-excel"), QStringLiteral("Spreadsheet")},
        {QStringLiteral("text/csv"), QStringLiteral("Spreadsheet")},
        {QStringLiteral("application/vnd.oasis.opendocument.text"), QStringLiteral("Document")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"), QStringLiteral("Document")},
        {QStringLiteral("application/msword"), QStringLiteral("Document")},
        {QStringLiteral("application/rtf"), QStringLiteral("Document")},
        {QStringLiteral("application/vnd.oasis.opendocument.presentation"), QStringLiteral("Presentation")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"), QStringLiteral("Presentation")},
        {QStringLiteral("application/vnd.ms-powerpoint"), QStringLiteral("Presentation")},
        {QStringLiteral("application/pdf"), QStringLiteral("PDF")},
        {QStringLiteral("text/plain"), QStringLiteral("Text")},
        {QStringLiteral("text/markdown"), QStringLiteral("Text")},
        {QStringLiteral("application/zip"), QStringLiteral("Compressed folder")},
        {QStringLiteral("application/x-desktop"), QStringLiteral("Shortcut")},
        {QStringLiteral("application/x-ms-dos-executable"), QStringLiteral("Windows app")},
        {QStringLiteral("application/x-msi"), QStringLiteral("Windows installer")},
        {QStringLiteral("application/x-ms-shortcut"), QStringLiteral("Windows shortcut")},
        {QStringLiteral("application/x-iso9660-image"), QStringLiteral("Disc image")},
        {QStringLiteral("application/vnd.debian.binary-package"), QStringLiteral("App package")},
        {QStringLiteral("application/octet-stream"), QStringLiteral("File")},
        {QStringLiteral("inode/symlink"), QStringLiteral("Shortcut")},
    };
    const auto it = plain.constFind(mime.name());
    if (it != plain.constEnd()) {
        return it.value();
    }
    QString comment = mime.comment();
    if (comment.isEmpty()) {
        return QStringLiteral("File");
    }
    // Shared MIME info writes "plain text document" and "JPEG image";
    // the first letter is raised and acronyms stay as they are.
    comment[0] = comment[0].toUpper();
    return comment;
}

QString date(const QDateTime &when)
{
    if (!when.isValid()) {
        return QString();
    }
    // The regional format chosen in Settings decides the order and the
    // clock; the words stay English, as the rest of Wild OS is.
    const QLocale names(QLocale::English, QLocale::UnitedKingdom);
    const QLocale format = QLocale::system();
    const QDateTime local = when.toLocalTime();
    const QDate today = QDate::currentDate();
    const QString time = format.toString(local.time(), QLocale::ShortFormat);
    const qint64 days = local.date().daysTo(today);
    if (days == 0) {
        return QStringLiteral("Today ") + time;
    }
    if (days == 1) {
        return QStringLiteral("Yesterday ") + time;
    }
    if (days > 1 && days < 7) {
        return names.dayName(local.date().dayOfWeek()) + QLatin1Char(' ') + time;
    }
    return names.toString(local.date(), format.dateFormat(QLocale::ShortFormat));
}

QString longDate(const QDateTime &when)
{
    if (!when.isValid()) {
        return QString();
    }
    const QDateTime local = when.toLocalTime();
    const QLocale names(QLocale::English, QLocale::UnitedKingdom);
    const QLocale format = QLocale::system();
    // Qt writes UK dates as "Monday, 9 November"; Wild OS has no comma.
    QString day = format.dateFormat(QLocale::LongFormat);
    if (day.startsWith(QLatin1String("dddd, d MMMM"))) {
        day.remove(4, 1);
    }
    return names.toString(local.date(), day) + QStringLiteral(", ")
        + format.toString(local.time(), QLocale::ShortFormat);
}

QString size(qint64 bytes)
{
    if (bytes < 0) {
        return QString();
    }
    // Separators from the regional format chosen in Settings (LC_NUMERIC).
    const QLocale numbers = QLocale::system();
    // Windows rounds small files up, so a 200 byte note is 1 KB, not 0.
    if (bytes < 1024 * 1024) {
        const qint64 kb = bytes == 0 ? 0 : (bytes + 1023) / 1024;
        return numbers.toString(kb) + QStringLiteral(" KB");
    }
    static const char *units[] = {"MB", "GB", "TB", "PB"};
    double value = bytes / (1024.0 * 1024.0);
    int unit = 0;
    while (value >= 1024 && unit < 3) {
        value /= 1024;
        ++unit;
    }
    // Three significant figures: 4.21 MB, 12.3 MB, 123 MB.
    const int decimals = value < 10 ? 2 : value < 100 ? 1 : 0;
    QString text = numbers.toString(value, 'f', decimals);
    if (text.contains(numbers.decimalPoint())) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(numbers.decimalPoint())) {
            text.chop(1);
        }
    }
    return text + QLatin1Char(' ') + QLatin1String(units[unit]);
}

QString exactSize(qint64 bytes)
{
    return size(bytes) + QStringLiteral(" (") + QLocale::system().toString(bytes) + (bytes == 1 ? QStringLiteral(" byte)") : QStringLiteral(" bytes)"));
}

QString count(qint64 n, const QString &one, const QString &many)
{
    return QLocale::system().toString(n) + QLatin1Char(' ') + (n == 1 ? one : many);
}

}
