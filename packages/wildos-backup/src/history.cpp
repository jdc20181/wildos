#include "history.h"
#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace
{
// Plenty for "See them all"; the snapshots on the drive are the record.
constexpr int kept = 400;

QJsonDocument readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll());
}

void writeJson(const QString &path, const QJsonDocument &doc)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(doc.toJson(QJsonDocument::Compact));
        f.commit();
    }
}
}

namespace History
{
QJsonArray load()
{
    return readJson(Paths::historyFile()).array();
}

void append(const QJsonObject &entry)
{
    QJsonArray list = load();
    list.prepend(entry);
    while (list.size() > kept) {
        list.removeLast();
    }
    writeJson(Paths::historyFile(), QJsonDocument(list));
}

QDateTime pendingSince()
{
    const QJsonObject o = readJson(Paths::pendingFile()).object();
    return QDateTime::fromString(o.value(QStringLiteral("since")).toString(), Qt::ISODate);
}

void setPending(const QDateTime &since)
{
    // The first missed run is the one a catch-up stands for.
    if (pendingSince().isValid()) {
        return;
    }
    writeJson(Paths::pendingFile(), QJsonDocument(QJsonObject{{QStringLiteral("since"), since.toString(Qt::ISODate)}}));
}

void clearPending()
{
    QFile::remove(Paths::pendingFile());
}
}
