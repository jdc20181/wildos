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
    const QDateTime local = when.toLocalTime();
    const QDate today = QDate::currentDate();
    const QString time = local.toString(QStringLiteral("HH:mm"));
    const qint64 days = local.date().daysTo(today);
    if (days == 0) {
        return QStringLiteral("Today ") + time;
    }
    if (days == 1) {
        return QStringLiteral("Yesterday ") + time;
    }
    if (days > 1 && days < 7) {
        return QLocale(QLocale::English, QLocale::UnitedKingdom).dayName(local.date().dayOfWeek()) + QLatin1Char(' ') + time;
    }
    return local.toString(QStringLiteral("dd/MM/yyyy"));
}

QString longDate(const QDateTime &when)
{
    if (!when.isValid()) {
        return QString();
    }
    const QDateTime local = when.toLocalTime();
    const QLocale uk(QLocale::English, QLocale::UnitedKingdom);
    return uk.toString(local, QStringLiteral("dddd d MMMM yyyy, HH:mm"));
}

QString size(qint64 bytes)
{
    if (bytes < 0) {
        return QString();
    }
    const QLocale uk(QLocale::English, QLocale::UnitedKingdom);
    // Windows rounds small files up, so a 200 byte note is 1 KB, not 0.
    if (bytes < 1024 * 1024) {
        const qint64 kb = bytes == 0 ? 0 : (bytes + 1023) / 1024;
        return uk.toString(kb) + QStringLiteral(" KB");
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
    QString text = uk.toString(value, 'f', decimals);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    return text + QLatin1Char(' ') + QLatin1String(units[unit]);
}

QString exactSize(qint64 bytes)
{
    const QLocale uk(QLocale::English, QLocale::UnitedKingdom);
    return size(bytes) + QStringLiteral(" (") + uk.toString(bytes) + (bytes == 1 ? QStringLiteral(" byte)") : QStringLiteral(" bytes)"));
}

QString count(qint64 n, const QString &one, const QString &many)
{
    const QLocale uk(QLocale::English, QLocale::UnitedKingdom);
    return uk.toString(n) + QLatin1Char(' ') + (n == 1 ? one : many);
}

}
