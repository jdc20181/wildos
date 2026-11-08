#include "launchwatcher.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace
{
const QString service = QStringLiteral("org.wildos.Launch");
const QString path = QStringLiteral("/org/wildos/Launch");
const QString interface = QStringLiteral("org.wildos.Launch");

// The daemon sends LaunchWaiting straight after LaunchStarted for a launch
// it follows, so an entry without one after this long is one it ignored.
constexpr qint64 unconfirmedMs = 2500;
// Its own give-up time is 60 s for a Windows app starting cold.
constexpr qint64 longestMs = 65000;

struct DesktopEntry {
    QString name;
    QString icon;
};

// Name and Icon from the [Desktop Entry] group. Only these two keys are
// needed, so this does without KConfig.
DesktopEntry readDesktopEntry(const QString &desktopId)
{
    DesktopEntry entry;
    const QString file = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId + QStringLiteral(".desktop"));
    QFile in(file);
    if (file.isEmpty() || !in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entry;
    }
    const QString local = QLocale().name();
    const QString language = local.section(QLatin1Char('_'), 0, 0);
    QString localName;
    bool inGroup = false;
    while (!in.atEnd()) {
        const QString line = QString::fromUtf8(in.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            if (inGroup) {
                break;
            }
            inGroup = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inGroup) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("Name")) {
            entry.name = value;
        } else if (key == QLatin1String("Name[%1]").arg(local) || (localName.isEmpty() && key == QLatin1String("Name[%1]").arg(language))) {
            localName = value;
        } else if (key == QLatin1String("Icon")) {
            entry.icon = value;
        }
    }
    if (!localName.isEmpty()) {
        entry.name = localName;
    }
    return entry;
}
}

LaunchWatcher::LaunchWatcher(QObject *parent)
    : QObject(parent)
    , m_expiry(new QTimer(this))
{
    m_expiry->setSingleShot(true);
    connect(m_expiry, &QTimer::timeout, this, &LaunchWatcher::expire);

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(service, path, interface, QStringLiteral("LaunchStarted"), this, SLOT(started(QString)));
    bus.connect(service, path, interface, QStringLiteral("LaunchWaiting"), this, SLOT(waiting(QString)));
    bus.connect(service, path, interface, QStringLiteral("LaunchEnded"), this, SLOT(ended(QString)));

    m_watcher = new QDBusServiceWatcher(service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        // A daemon that went away (or restarted) is no longer opening
        // anything it announced before.
        if (!m_entries.isEmpty()) {
            m_entries.clear();
            Q_EMIT launchesChanged();
        }
        loadState();
    });

    // plasmashell can restart while an app is on its way.
    loadState();
}

QString LaunchWatcher::normalise(const QString &key)
{
    QString id = key;
    if (id.contains(QLatin1Char(':'))) {
        // applications:org.kde.kwrite.desktop or file:///usr/share/applications/org.kde.kwrite.desktop
        const QUrl url(id);
        id = url.isLocalFile() ? url.fileName() : url.path().section(QLatin1Char('/'), -1);
    }
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return id.toLower();
}

QVariantList LaunchWatcher::launches() const
{
    QList<QString> ids = m_entries.keys();
    std::sort(ids.begin(), ids.end(), [this](const QString &a, const QString &b) {
        return m_entries[a].order < m_entries[b].order;
    });
    QVariantList list;
    for (const QString &id : std::as_const(ids)) {
        const Entry &entry = m_entries[id];
        list.append(QVariantMap{
            {QStringLiteral("desktopId"), id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("icon"), entry.icon},
            {QStringLiteral("confirmed"), entry.confirmed},
        });
    }
    return list;
}

bool LaunchWatcher::isStarting(const QString &key) const
{
    return !key.isEmpty() && m_entries.contains(normalise(key));
}

void LaunchWatcher::add(const QString &desktopId, bool confirmed)
{
    const QString id = normalise(desktopId);
    if (id.isEmpty()) {
        return;
    }
    auto it = m_entries.find(id);
    if (it == m_entries.end()) {
        const DesktopEntry desktop = readDesktopEntry(desktopId.endsWith(QLatin1String(".desktop")) ? desktopId.chopped(8) : desktopId);
        Entry entry;
        entry.name = desktop.name;
        entry.icon = desktop.icon;
        entry.order = ++m_next;
        it = m_entries.insert(id, entry);
    }
    it->since = QDateTime::currentMSecsSinceEpoch();
    it->confirmed = it->confirmed || confirmed;
    scheduleExpiry();
    Q_EMIT launchesChanged();
}

void LaunchWatcher::started(const QString &desktopId)
{
    add(desktopId, false);
}

void LaunchWatcher::waiting(const QString &desktopId)
{
    add(desktopId, true);
}

void LaunchWatcher::ended(const QString &desktopId)
{
    if (m_entries.remove(normalise(desktopId)) > 0) {
        scheduleExpiry();
        Q_EMIT launchesChanged();
    }
}

void LaunchWatcher::expire()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        const qint64 age = now - it->since;
        if (age >= longestMs || (!it->confirmed && age >= unconfirmedMs)) {
            it = m_entries.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    scheduleExpiry();
    if (changed) {
        Q_EMIT launchesChanged();
    }
}

void LaunchWatcher::scheduleExpiry()
{
    if (m_entries.isEmpty()) {
        m_expiry->stop();
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 soonest = longestMs;
    for (const Entry &entry : std::as_const(m_entries)) {
        const qint64 limit = entry.confirmed ? longestMs : unconfirmedMs;
        soonest = std::min(soonest, std::max<qint64>(0, entry.since + limit - now));
    }
    m_expiry->start(int(soonest) + 10);
}

void LaunchWatcher::loadState()
{
    QDBusMessage call = QDBusMessage::createMethodCall(service, path, interface, QStringLiteral("State"));
    auto *pending = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QString> reply = *watcher;
        if (reply.isError()) {
            return;
        }
        const QJsonArray running = QJsonDocument::fromJson(reply.value().toUtf8()).object().value(QStringLiteral("launches")).toArray();
        for (const QJsonValue &value : running) {
            const QJsonObject launch = value.toObject();
            // Every launch in the daemon's list is one it is waiting for.
            add(launch.value(QStringLiteral("desktopId")).toString(), true);
        }
    });
}
