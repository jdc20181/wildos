#include "positions.h"
#include "paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace
{
Positions *s_instance = nullptr;

// A few seconds in is not worth coming back to, and neither are the
// closing credits.
constexpr double minimumResume = 5.0;
constexpr double watchedShare = 0.95;
// The file stays small: the oldest entries go first.
constexpr int keepEntries = 2000;
}

Positions::Positions(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_save.setSingleShot(true);
    m_save.setInterval(2000);
    connect(&m_save, &QTimer::timeout, this, &Positions::save);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &Positions::save);
    load();
}

Positions *Positions::instance()
{
    if (!s_instance) {
        new Positions(QCoreApplication::instance());
    }
    return s_instance;
}

Positions *Positions::create(QQmlEngine *, QJSEngine *)
{
    Positions *positions = instance();
    QQmlEngine::setObjectOwnership(positions, QQmlEngine::CppOwnership);
    return positions;
}

double Positions::resumeAt(const QString &path) const
{
    const auto it = m_entries.constFind(path);
    if (it == m_entries.cend() || it->watched || it->position < minimumResume) {
        return 0;
    }
    return it->position;
}

void Positions::remember(const QString &path, double position, double duration)
{
    if (path.isEmpty() || !(duration > 0)) {
        return;
    }
    Entry &entry = m_entries[path];
    entry.duration = duration;
    entry.at = QDateTime::currentMSecsSinceEpoch();
    if (position >= duration * watchedShare) {
        entry.watched = true;
        entry.position = 0;
    } else if (position >= minimumResume) {
        entry.watched = false;
        entry.position = position;
    } else if (!entry.watched) {
        entry.position = 0;
    }
    touch();
}

void Positions::markWatched(const QString &path, double duration)
{
    remember(path, duration, duration);
}

void Positions::setWatched(const QString &path, bool watched)
{
    if (path.isEmpty()) {
        return;
    }
    Entry &entry = m_entries[path];
    entry.watched = watched;
    entry.position = 0;
    entry.at = QDateTime::currentMSecsSinceEpoch();
    touch();
}

double Positions::progress(const QString &path) const
{
    const auto it = m_entries.constFind(path);
    if (it == m_entries.cend()) {
        return 0;
    }
    if (it->watched) {
        return 1;
    }
    return it->duration > 0 ? std::clamp(it->position / it->duration, 0.0, 1.0) : 0;
}

bool Positions::watched(const QString &path) const
{
    const auto it = m_entries.constFind(path);
    return it != m_entries.cend() && it->watched;
}

QStringList Positions::recent(int count) const
{
    QList<QPair<qint64, QString>> list;
    list.reserve(m_entries.size());
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        list.append({it->at, it.key()});
    }
    std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });
    QStringList paths;
    for (const auto &item : std::as_const(list)) {
        if (paths.size() >= count) {
            break;
        }
        if (QFileInfo::exists(item.second)) {
            paths.append(item.second);
        }
    }
    return paths;
}

void Positions::touch()
{
    ++m_revision;
    Q_EMIT changed();
    m_save.start();
}

void Positions::load()
{
    QFile file(VideosPaths::positionsFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject videos = root.value(QLatin1String("videos")).toObject();
    for (auto it = videos.begin(); it != videos.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        Entry entry;
        entry.position = o.value(QLatin1String("position")).toDouble();
        entry.duration = o.value(QLatin1String("duration")).toDouble();
        entry.watched = o.value(QLatin1String("watched")).toBool();
        entry.at = qint64(o.value(QLatin1String("at")).toDouble());
        m_entries.insert(it.key(), entry);
    }
}

void Positions::save()
{
    m_save.stop();
    if (m_entries.size() > keepEntries) {
        QList<QPair<qint64, QString>> byAge;
        for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
            byAge.append({it->at, it.key()});
        }
        std::sort(byAge.begin(), byAge.end());
        for (qsizetype i = 0; i < byAge.size() - keepEntries; ++i) {
            m_entries.remove(byAge.at(i).second);
        }
    }
    QJsonObject videos;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        videos.insert(it.key(), QJsonObject{
                                    {QStringLiteral("position"), it->position},
                                    {QStringLiteral("duration"), it->duration},
                                    {QStringLiteral("watched"), it->watched},
                                    {QStringLiteral("at"), double(it->at)},
                                });
    }
    QDir().mkpath(VideosPaths::dataFolder());
    QSaveFile file(VideosPaths::positionsFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("videos"), videos}}).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
