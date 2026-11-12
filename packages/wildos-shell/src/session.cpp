#include "session.h"

#include <KScreenDpms/Dpms>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QScreen>

namespace
{
const QString screenSaverService = QStringLiteral("org.freedesktop.ScreenSaver");
const QString screenSaverPath = QStringLiteral("/ScreenSaver");
const QString profilesService = QStringLiteral("org.freedesktop.UPower.PowerProfiles");
const QString profilesPath = QStringLiteral("/org/freedesktop/UPower/PowerProfiles");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
}

Session::Session(QObject *parent)
    : QObject(parent)
    , m_dpms(new KScreen::Dpms(this))
{
    QDBusConnection::sessionBus().connect(screenSaverService, screenSaverPath, screenSaverService, QStringLiteral("ActiveChanged"), this,
                                          SLOT(setLocked(bool)));
    QDBusConnection::systemBus().connect(profilesService, profilesPath, propertiesInterface, QStringLiteral("PropertiesChanged"), this,
                                         SLOT(profilesPropertiesChanged(QString, QVariantMap, QStringList)));

    // KWin reports each screen's power state as it changes, and once for
    // every screen when the protocol is bound.
    connect(m_dpms, &KScreen::Dpms::modeChanged, this, [this](KScreen::Dpms::Mode mode, QScreen *screen) {
        if (screen) {
            setScreenOff(screen->name(), mode != KScreen::Dpms::On);
        }
    });
    // A screen unplugged while off would otherwise stay listed, and a new
    // one can take its name.
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        setScreenOff(screen->name(), false);
    });

    readLocked();
    readProfile();
}

void Session::readLocked()
{
    const QDBusMessage call = QDBusMessage::createMethodCall(screenSaverService, screenSaverPath, screenSaverService, QStringLiteral("GetActive"));
    // Asynchronous, so a slow screen locker never holds up plasmashell's
    // start; until it answers the screen counts as unlocked.
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<bool> reply = *watcher;
        if (!reply.isError()) {
            setLocked(reply.value());
        }
    });
}

void Session::readProfile()
{
    QDBusMessage call = QDBusMessage::createMethodCall(profilesService, profilesPath, propertiesInterface, QStringLiteral("Get"));
    call << profilesService << QStringLiteral("ActiveProfile");
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (!reply.isError()) {
            setPowerSaver(reply.value().variant().toString() == QLatin1String("power-saver"));
        }
    });
}

void Session::profilesPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != profilesService) {
        return;
    }
    const auto it = changed.constFind(QStringLiteral("ActiveProfile"));
    if (it != changed.constEnd()) {
        setPowerSaver(it->toString() == QLatin1String("power-saver"));
    } else if (invalidated.contains(QLatin1String("ActiveProfile"))) {
        readProfile();
    }
}

void Session::setLocked(bool locked)
{
    if (locked != m_locked) {
        m_locked = locked;
        Q_EMIT lockedChanged();
    }
}

void Session::setPowerSaver(bool powerSaver)
{
    if (powerSaver != m_powerSaver) {
        m_powerSaver = powerSaver;
        Q_EMIT powerSaverChanged();
    }
}

void Session::setScreenOff(const QString &name, bool off)
{
    if (off == m_screensOff.contains(name)) {
        return;
    }
    if (off) {
        m_screensOff.append(name);
    } else {
        m_screensOff.removeAll(name);
    }
    Q_EMIT screensOffChanged();
}
