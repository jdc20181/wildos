#include "inputdevices.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

namespace
{
const QString service = QStringLiteral("org.kde.KWin");
const QString managerPath = QStringLiteral("/org/kde/KWin/InputDevice");
const QString managerInterface = QStringLiteral("org.kde.KWin.InputDeviceManager");
const QString deviceInterface = QStringLiteral("org.kde.KWin.InputDevice");
const QString properties = QStringLiteral("org.freedesktop.DBus.Properties");

QVariant unwrap(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }
    return value;
}

QVariantMap allProperties(const QString &path)
{
    auto message = QDBusMessage::createMethodCall(service, path, properties, QStringLiteral("GetAll"));
    message << deviceInterface;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    QVariantMap map;
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg >> map;
    for (auto it = map.begin(); it != map.end(); ++it) {
        it.value() = unwrap(it.value());
    }
    return map;
}
}

InputDevices::InputDevices(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(service, managerPath, managerInterface, QStringLiteral("deviceAdded"), this, SLOT(onDevicesChanged()));
    bus.connect(service, managerPath, managerInterface, QStringLiteral("deviceRemoved"), this, SLOT(onDevicesChanged()));
    reload();
}

void InputDevices::onDevicesChanged()
{
    reload();
}

void InputDevices::reload()
{
    auto message = QDBusMessage::createMethodCall(service, managerPath, properties, QStringLiteral("Get"));
    message << managerInterface << QStringLiteral("devicesSysNames");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    QStringList names;
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        names = unwrap(reply.arguments().constFirst()).toStringList();
    }

    m_mice.clear();
    m_touchpads.clear();
    for (const QString &name : std::as_const(names)) {
        QVariantMap device = allProperties(managerPath + QLatin1Char('/') + name);
        if (device.isEmpty()) {
            continue;
        }
        device.insert(QStringLiteral("sysName"), name);
        if (device.value(QStringLiteral("touchpad")).toBool()) {
            m_touchpads << device;
        } else if (device.value(QStringLiteral("pointer")).toBool() && !device.value(QStringLiteral("tabletTool")).toBool()) {
            m_mice << device;
        }
    }
    Q_EMIT changed();
}

void InputDevices::set(const QString &kind, const QString &property, const QVariant &value)
{
    const QVariantList &list = kind == QLatin1String("touchpad") ? m_touchpads : m_mice;
    for (const QVariant &entry : list) {
        const QString sysName = entry.toMap().value(QStringLiteral("sysName")).toString();
        auto message = QDBusMessage::createMethodCall(service, managerPath + QLatin1Char('/') + sysName, properties, QStringLiteral("Set"));
        message << deviceInterface << property << QVariant::fromValue(QDBusVariant(value));
        QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    }
    reload();
}
