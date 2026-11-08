#include "store.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double halfLifeDays = 14.0;
}

JsonFile::JsonFile(const QString &name, QObject *parent)
    : QObject(parent)
    , m_path(directory() + QLatin1Char('/') + name)
    , m_data(read(name))
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &JsonFile::flush);
}

QString JsonFile::directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/wildos/launch");
}

QJsonObject JsonFile::read(const QString &name)
{
    QFile file(directory() + QLatin1Char('/') + name);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void JsonFile::setData(const QJsonObject &data)
{
    m_data = data;
    m_timer.start();
}

void JsonFile::flush()
{
    m_timer.stop();
    QDir().mkpath(directory());
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_data).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

UsageStore::UsageStore(QObject *parent)
    : QObject(parent)
    , m_file(QStringLiteral("usage.json"))
{
}

double UsageStore::score(const QJsonObject &app, qint64 now)
{
    const double days = std::max<qint64>(0, now - app.value(QStringLiteral("last")).toInteger()) / 86400.0;
    return app.value(QStringLiteral("score")).toDouble() * std::pow(0.5, days / halfLifeDays);
}

QStringList UsageStore::ranked(const QJsonObject &apps)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<QPair<double, QString>> scored;
    for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
        scored.append({score(it.value().toObject(), now), it.key()});
    }
    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });
    QStringList ids;
    for (const auto &entry : std::as_const(scored)) {
        ids << entry.second;
    }
    return ids;
}

void UsageStore::countLaunch(const QString &desktopId)
{
    QJsonObject data = m_file.data();
    QJsonObject apps = data.value(QStringLiteral("apps")).toObject();
    QJsonObject app = apps.value(desktopId).toObject();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    app.insert(QStringLiteral("score"), score(app, now) + 1.0);
    app.insert(QStringLiteral("count"), app.value(QStringLiteral("count")).toInt() + 1);
    app.insert(QStringLiteral("last"), now);
    apps.insert(desktopId, app);
    data.insert(QStringLiteral("apps"), apps);
    m_file.setData(data);
}

void UsageStore::setFiles(const QString &desktopId, const QJsonArray &files)
{
    QJsonObject data = m_file.data();
    QJsonObject apps = data.value(QStringLiteral("apps")).toObject();
    QJsonObject app = apps.value(desktopId).toObject();
    if (app.isEmpty()) {
        return;
    }
    app.insert(QStringLiteral("files"), files);
    apps.insert(desktopId, app);
    data.insert(QStringLiteral("apps"), apps);
    m_file.setData(data);
}
