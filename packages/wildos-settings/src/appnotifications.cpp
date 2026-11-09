#include "appnotifications.h"

#include <KApplicationTrader>
#include <KConfig>
#include <KConfigGroup>
#include <KService>
#include <KSharedConfig>

#include <QCollator>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace
{
const QString soundAction = QStringLiteral("Sound");

KConfigGroup applicationGroup(const KSharedConfigPtr &config, const QString &app)
{
    return config->group(QStringLiteral("Applications")).group(app);
}

// The app's own events file, as installed. KNotification reads it under
// the user's file, which only holds what the user changed.
QString eventsFile(const QString &notifyRc)
{
    const QString name = notifyRc + QStringLiteral(".notifyrc");
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("knotifications6/") + name);
    if (path.isEmpty()) {
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("knotifications5/") + name);
    }
    return path;
}

KSharedConfigPtr userEvents(const QString &notifyRc)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(notifyRc + QStringLiteral(".notifyrc"), KConfig::NoGlobals);
    config->reparseConfiguration();
    return config;
}

QStringList split(const QString &actions)
{
    return actions.split(QLatin1Char('|'), Qt::SkipEmptyParts);
}

QSet<QString> asSet(const QStringList &list)
{
    return QSet<QString>(list.cbegin(), list.cend());
}

struct Sound {
    bool available = false;
    bool on = false;
};

// Only events that play a sound out of the box count: an app's quiet
// events stay quiet when its sound is turned on.
Sound soundOf(const QString &notifyRc)
{
    Sound sound;
    const QString path = eventsFile(notifyRc);
    if (path.isEmpty()) {
        return sound;
    }
    const KConfig defaults(path, KConfig::SimpleConfig);
    const KSharedConfigPtr user = userEvents(notifyRc);
    const QStringList groups = defaults.groupList();
    for (const QString &name : groups) {
        if (!name.startsWith(QLatin1String("Event/"))) {
            continue;
        }
        const QString fallback = defaults.group(name).readEntry("Action");
        if (!split(fallback).contains(soundAction)) {
            continue;
        }
        sound.available = true;
        const KConfigGroup mine = user->group(name);
        const QString actions = mine.hasKey("Action") ? mine.readEntry("Action") : fallback;
        if (split(actions).contains(soundAction)) {
            sound.on = true;
        }
    }
    return sound;
}

KService::Ptr appService(const QString &desktopEntry)
{
    if (desktopEntry.isEmpty()) {
        return {};
    }
    KService::Ptr service = KService::serviceByDesktopName(desktopEntry);
    if (!service || service->noDisplay() || service->name().isEmpty()) {
        return {};
    }
    return service;
}
}

AppNotifications::AppNotifications(QObject *parent)
    : QObject(parent)
{
}

// The same three sources kcm_notifications lists apps from, in its order,
// so an app with an events file keeps its sound switch.
void AppNotifications::reload()
{
    m_list.clear();
    QSet<QString> seen;

    const auto add = [this, &seen](const KService::Ptr &service, const QString &notifyRc) {
        const QString id = service->desktopEntryName();
        if (seen.contains(id)) {
            return;
        }
        seen.insert(id);
        m_list.append(App{id, service->name(), service->icon(), notifyRc});
    };

    QSet<QString> files;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("knotifications6"), QStandardPaths::LocateDirectory)
        + QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("knotifications5"), QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        const QStringList names = QDir(dir).entryList({QStringLiteral("*.notifyrc")}, QDir::Files);
        for (const QString &file : names) {
            if (files.contains(file)) {
                continue;
            }
            files.insert(file);
            const KConfig events(dir + QLatin1Char('/') + file, KConfig::SimpleConfig);
            const QString desktopEntry = events.group(QStringLiteral("Global")).readEntry("DesktopEntry");
            // No desktop entry means a background service (the desktop's own
            // parts), which Windows does not list either.
            if (const KService::Ptr service = appService(desktopEntry)) {
                add(service, file.section(QLatin1Char('.'), 0, -2));
            }
        }
    }

    const KService::List announced = KApplicationTrader::query([](const KService::Ptr &service) {
        return !service->noDisplay() && service->property<bool>(QStringLiteral("X-GNOME-UsesNotifications"));
    });
    for (const KService::Ptr &service : announced) {
        if (!service->name().isEmpty()) {
            add(service, {});
        }
    }

    // Apps the notification server has heard from.
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"));
    config->reparseConfiguration();
    const QStringList known = config->group(QStringLiteral("Applications")).groupList();
    for (const QString &desktopEntry : known) {
        if (const KService::Ptr service = appService(desktopEntry)) {
            add(service, {});
        }
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(m_list.begin(), m_list.end(), [&collator](const App &a, const App &b) {
        return collator.compare(a.name, b.name) < 0;
    });

    publish();
}

void AppNotifications::publish()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"));
    config->reparseConfiguration();

    m_apps.clear();
    for (const App &app : std::as_const(m_list)) {
        const KConfigGroup group = applicationGroup(config, app.id);
        const bool banners = group.readEntry("ShowPopups", true);
        const bool history = group.readEntry("ShowInHistory", true);
        const Sound sound = app.notifyRc.isEmpty() ? Sound{} : soundOf(app.notifyRc);
        m_apps.append(QVariantMap{
            {QStringLiteral("id"), app.id},
            {QStringLiteral("name"), app.name},
            {QStringLiteral("icon"), app.icon},
            {QStringLiteral("enabled"), banners || history},
            {QStringLiteral("banners"), banners},
            {QStringLiteral("history"), history},
            {QStringLiteral("hasSound"), sound.available},
            {QStringLiteral("sound"), sound.on},
        });
    }
    Q_EMIT appsChanged();
}

QString AppNotifications::notifyRcFor(const QString &app) const
{
    for (const App &entry : m_list) {
        if (entry.id == app) {
            return entry.notifyRc;
        }
    }
    return {};
}

// As NotificationManager::Settings writes them: a value that matches the
// default is removed rather than written, and every write carries Notify
// so plasmashell's config watcher applies it straight away.
void AppNotifications::writeBehaviour(const QString &app, const QString &key, bool on)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"));
    KConfigGroup group = applicationGroup(config, app);
    if (on && !group.hasDefault(key)) {
        group.revertToDefault(key, KConfig::Notify);
    } else {
        group.writeEntry(key, on, KConfig::Notify);
    }
    config->sync();
}

void AppNotifications::writeSound(const QString &notifyRc, bool on)
{
    const QString path = eventsFile(notifyRc);
    if (path.isEmpty()) {
        return;
    }
    const KConfig defaults(path, KConfig::SimpleConfig);
    KSharedConfigPtr user = userEvents(notifyRc);
    const QStringList groups = defaults.groupList();
    for (const QString &name : groups) {
        if (!name.startsWith(QLatin1String("Event/"))) {
            continue;
        }
        const QString fallback = defaults.group(name).readEntry("Action");
        if (!split(fallback).contains(soundAction)) {
            continue;
        }
        KConfigGroup mine = user->group(name);
        QStringList actions = split(mine.hasKey("Action") ? mine.readEntry("Action") : fallback);
        actions.removeAll(soundAction);
        if (on) {
            actions.append(soundAction);
        }
        // Back to exactly what the app ships means no override at all, so a
        // later update to the app's own events still reaches the user.
        if (asSet(actions) == asSet(split(fallback))) {
            mine.deleteEntry("Action");
        } else {
            mine.writeEntry("Action", actions.join(QLatin1Char('|')));
        }
    }
    user->sync();

    // What kcm_notifications sends after saving: every running app with
    // these events rereads them.
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/Config"), QStringLiteral("org.kde.knotification"), QStringLiteral("reparseConfiguration"));
    message.setArguments({notifyRc});
    QDBusConnection::sessionBus().send(message);
}

void AppNotifications::setEnabled(const QString &app, bool on)
{
    writeBehaviour(app, QStringLiteral("ShowPopups"), on);
    writeBehaviour(app, QStringLiteral("ShowInHistory"), on);
    writeBehaviour(app, QStringLiteral("ShowBadges"), on);
    const QString notifyRc = notifyRcFor(app);
    if (!notifyRc.isEmpty()) {
        writeSound(notifyRc, on);
    }
    publish();
}

void AppNotifications::setBanners(const QString &app, bool on)
{
    writeBehaviour(app, QStringLiteral("ShowPopups"), on);
    publish();
}

void AppNotifications::setHistory(const QString &app, bool on)
{
    writeBehaviour(app, QStringLiteral("ShowInHistory"), on);
    publish();
}

void AppNotifications::setSound(const QString &app, bool on)
{
    const QString notifyRc = notifyRcFor(app);
    if (!notifyRc.isEmpty()) {
        writeSound(notifyRc, on);
    }
    publish();
}
