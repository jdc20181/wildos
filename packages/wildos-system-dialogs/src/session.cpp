#include "session.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

namespace
{
QDBusMessage login1(const QString &path, const QString &interface, const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), path, interface, method);
}
}

QString SessionInfo::id()
{
    const QString env = qEnvironmentVariable("XDG_SESSION_ID");
    if (!env.isEmpty()) {
        return env;
    }
    auto message = login1(QStringLiteral("/org/freedesktop/login1/user/self"), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    message << QStringLiteral("org.freedesktop.login1.User") << QStringLiteral("Display");
    const QDBusMessage reply = QDBusConnection::systemBus().call(message, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusVariant>().variant().value<QDBusArgument>();
    QString id;
    QDBusObjectPath path;
    arg.beginStructure();
    arg >> id >> path;
    arg.endStructure();
    return id;
}

QDBusObjectPath SessionInfo::path()
{
    const QString session = id();
    if (session.isEmpty()) {
        return {};
    }
    auto message = login1(QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("GetSession"));
    message << session;
    const QDBusMessage reply = QDBusConnection::systemBus().call(message, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return reply.arguments().constFirst().value<QDBusObjectPath>();
}

bool DialogsService::forward(const QString &method)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath), QString::fromLatin1(serviceName), method);
    // No auto-start: when the service is gone this process shows the
    // screen itself rather than waiting for one to start.
    message.setAutoStartService(false);
    const QDBusMessage reply = bus.call(message, QDBus::Block, 1000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

void DialogsService::ShowSecurity()
{
    Q_EMIT securityRequested();
}

void DialogsService::ShowRestart()
{
    Q_EMIT restartRequested();
}

QString DialogsService::AskWindowsShutdown(const QString &action, const QString &appsJson)
{
    setDelayedReply(true);
    Q_EMIT windowsShutdownRequested(action, appsJson, message());
    return {};
}

QString DialogsService::AskWindowsDevices(const QString &appName, const QString &devicesJson)
{
    setDelayedReply(true);
    Q_EMIT windowsDevicesRequested(appName, devicesJson, message());
    return {};
}

void DialogsService::ShowWindowsStarting(const QString &appName, const QString &iconName, int seconds, bool cold)
{
    Q_EMIT windowsStartingRequested(appName, iconName, seconds, cold);
}

void DialogsService::HideWindowsStarting()
{
    Q_EMIT windowsStartingDone();
}
