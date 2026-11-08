#pragma once

#include <QDateTime>
#include <QMimeType>
#include <QString>

// The house style for names, types, dates and sizes, copied from
// wildos-files so the dialog names things as Files does (PDF alone reads
// "PDF document", as the Save as board writes it). Extensions
// hidden for known types, plain type words, day-first dates, sizes the way
// Windows writes them.
namespace Format
{
QString visibleName(const QString &fileName, bool isDir, bool showExtensions);
QString typeName(const QMimeType &mime, bool isDir);
QString date(const QDateTime &when);
QString longDate(const QDateTime &when);
QString size(qint64 bytes);
QString exactSize(qint64 bytes);
QString count(qint64 n, const QString &one, const QString &many);
}
