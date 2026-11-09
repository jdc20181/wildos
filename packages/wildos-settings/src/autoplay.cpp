#include "autoplay.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCollator>
#include <QDBusConnection>
#include <QDBusMessage>

#include <algorithm>

namespace
{
const QString configFile = QStringLiteral("kded_device_automounterrc");
const QString module = QStringLiteral("device_automounter");

KSharedConfigPtr config()
{
    return KSharedConfig::openConfig(configFile, KConfig::NoGlobals);
}

void callKded(const QString &method, const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kded6"), QStringLiteral("/kded"), QStringLiteral("org.kde.kded6"), method);
    message.setArguments(arguments);
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}
}

AutoPlay::AutoPlay(QObject *parent)
    : QObject(parent)
{
    reload();
}

void AutoPlay::reload()
{
    KSharedConfigPtr cfg = config();
    // The automounter writes here as drives come and go.
    cfg->reparseConfiguration();
    const KConfigGroup general(cfg, QStringLiteral("General"));
    m_onPlugin = general.readEntry("AutomountOnPlugin", false);
    m_onLogin = general.readEntry("AutomountOnLogin", false);
    m_unknownDevices = general.readEntry("AutomountUnknownDevices", false);

    m_drives.clear();
    const KConfigGroup devices(cfg, QStringLiteral("Devices"));
    const QStringList udis = devices.groupList();
    for (const QString &udi : udis) {
        const KConfigGroup device = devices.group(udi);
        QString name = device.readEntry("LastNameSeen", QString());
        if (name.isEmpty()) {
            name = udi.section(QLatin1Char('/'), -1);
        }
        const bool always = device.readEntry("ForceAttachAutomount", false) || device.readEntry("ForceLoginAutomount", false);
        m_drives.append(QVariantMap{{QStringLiteral("udi"), udi}, {QStringLiteral("name"), name}, {QStringLiteral("always"), always}});
    }
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(m_drives.begin(), m_drives.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("name")).toString(), b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    Q_EMIT changed();
}

void AutoPlay::save()
{
    KSharedConfigPtr cfg = config();
    KConfigGroup general(cfg, QStringLiteral("General"));
    general.writeEntry("AutomountOnPlugin", m_onPlugin);
    general.writeEntry("AutomountOnLogin", m_onLogin);
    general.writeEntry("AutomountUnknownDevices", m_unknownDevices);

    // The automounter unloads itself at start when AutomountEnabled is
    // false, so it is on whenever anything at all should be mounted,
    // worked out the way KDE's module does.
    bool enabled = m_onPlugin || m_onLogin;
    const KConfigGroup devices(cfg, QStringLiteral("Devices"));
    const QStringList udis = devices.groupList();
    for (const QString &udi : udis) {
        const KConfigGroup device = devices.group(udi);
        enabled = enabled || device.readEntry("ForceAttachAutomount", false) || device.readEntry("ForceLoginAutomount", false);
    }
    general.writeEntry("AutomountEnabled", enabled);
    cfg->sync();

    callKded(QStringLiteral("setModuleAutoloading"), {module, enabled});
    callKded(enabled ? QStringLiteral("loadModule") : QStringLiteral("unloadModule"), {module});
    reload();
}

void AutoPlay::setOnPlugin(bool on)
{
    m_onPlugin = on;
    save();
}

void AutoPlay::setOnLogin(bool on)
{
    m_onLogin = on;
    save();
}

void AutoPlay::setUnknownDevices(bool on)
{
    m_unknownDevices = on;
    save();
}

void AutoPlay::setAlways(const QString &udi, bool always)
{
    KConfigGroup device = KConfigGroup(config(), QStringLiteral("Devices")).group(udi);
    if (!device.exists()) {
        return;
    }
    device.writeEntry("ForceAttachAutomount", always);
    device.writeEntry("ForceLoginAutomount", always);
    save();
}

void AutoPlay::forget(const QString &udi)
{
    KConfigGroup device = KConfigGroup(config(), QStringLiteral("Devices")).group(udi);
    if (device.exists()) {
        device.deleteGroup();
    }
    save();
}
