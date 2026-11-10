#include "underglow.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace
{
const QString upowerService = QStringLiteral("org.freedesktop.UPower");
const QString upowerPath = QStringLiteral("/org/freedesktop/UPower");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
}

Underglow::Underglow(QObject *parent)
    : QObject(parent)
    , m_wildos(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc"))))
    , m_globals(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals"))))
{
    connect(m_wildos.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Underglow")) {
            readSettings();
        }
    });
    connect(m_globals.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("KDE")) {
            readMotion();
        }
    });

    // UPower sends OnBattery changes as a properties signal; a laptop
    // unplugged mid-breath settles to steady straight away.
    QDBusConnection::systemBus().connect(upowerService, upowerPath, propertiesInterface, QStringLiteral("PropertiesChanged"), this,
                                         SLOT(upowerPropertiesChanged(QString, QVariantMap, QStringList)));

    readSettings();
    readMotion();
    readBattery();
}

void Underglow::readSettings()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const KConfigGroup group = config->group(QStringLiteral("Underglow"));
    const UnderglowSettings::Settings settings = UnderglowSettings::parse(group.readEntry("Enabled", false),
                                                                          group.readEntry("Colour", QString()),
                                                                          group.readEntry("Custom", QString()),
                                                                          group.readEntry("Effect", QString()));
    if (settings.enabled != m_settings.enabled || settings.colour != m_settings.colour || settings.custom != m_settings.custom
        || settings.effect != m_settings.effect) {
        m_settings = settings;
        Q_EMIT settingsChanged();
        update();
    }
}

void Underglow::readMotion()
{
    // Settings > Accessibility > Reduce motion sets KDE's animation speed to
    // instant, the same key WildOS.UI's Theme follows.
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    const bool reduced = config->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0) <= 0.0;
    if (reduced != m_reducedMotion) {
        m_reducedMotion = reduced;
        Q_EMIT reducedMotionChanged();
        update();
    }
}

void Underglow::readBattery()
{
    QDBusMessage call = QDBusMessage::createMethodCall(upowerService, upowerPath, propertiesInterface, QStringLiteral("Get"));
    call << upowerService << QStringLiteral("OnBattery");
    // Asynchronous, so a missing or slow UPower never holds up KWin; until
    // it answers the glow counts as on mains power.
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (!reply.isError()) {
            setOnBattery(reply.value().variant().toBool());
        }
    });
}

void Underglow::upowerPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != upowerService) {
        return;
    }
    const auto it = changed.constFind(QStringLiteral("OnBattery"));
    if (it != changed.constEnd()) {
        setOnBattery(it->toBool());
    } else if (invalidated.contains(QLatin1String("OnBattery"))) {
        readBattery();
    }
}

void Underglow::setOnBattery(bool onBattery)
{
    if (onBattery != m_onBattery) {
        m_onBattery = onBattery;
        Q_EMIT onBatteryChanged();
        update();
    }
}

void Underglow::setFullScreen(bool fullScreen)
{
    if (fullScreen != m_fullScreen) {
        m_fullScreen = fullScreen;
        Q_EMIT fullScreenChanged();
        update();
    }
}

void Underglow::update()
{
    const QString running = UnderglowSettings::effectName(UnderglowSettings::running(m_settings, m_onBattery, m_fullScreen, m_reducedMotion));
    if (running != m_running) {
        m_running = running;
        Q_EMIT runningChanged();
    }
}
