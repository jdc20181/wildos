#include "sidecar.h"

#include "paths.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

Sidecar Sidecar::read(const QString &opusPath)
{
    Sidecar sidecar;
    QFile file(VoicePaths::sidecarOf(opusPath));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) {
        return sidecar;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    sidecar.title = object.value(QStringLiteral("title")).toString();
    sidecar.created = QDateTime::fromString(object.value(QStringLiteral("created")).toString(), Qt::ISODate);
    sidecar.durationMs = qint64(object.value(QStringLiteral("duration")).toDouble(-1));
    for (const QJsonValue &marker : object.value(QStringLiteral("markers")).toArray()) {
        if (marker.isDouble() && marker.toDouble() >= 0) {
            sidecar.markers.append(qint64(marker.toDouble()));
        }
    }
    return sidecar;
}

bool Sidecar::write(const QString &opusPath) const
{
    QJsonObject object;
    if (!title.isEmpty()) {
        object.insert(QStringLiteral("title"), title);
    }
    if (created.isValid()) {
        object.insert(QStringLiteral("created"), created.toUTC().toString(Qt::ISODateWithMs));
    }
    if (durationMs >= 0) {
        object.insert(QStringLiteral("duration"), double(durationMs));
    }
    QJsonArray list;
    for (qint64 marker : markers) {
        list.append(double(marker));
    }
    object.insert(QStringLiteral("markers"), list);
    object.insert(QStringLiteral("app"), QStringLiteral("Wild OS Voice"));
    QSaveFile file(VoicePaths::sidecarOf(opusPath));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}
