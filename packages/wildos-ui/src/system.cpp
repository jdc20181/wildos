#include "system.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>
#include <QStandardPaths>

#include <unistd.h>

namespace
{
const QString windowMenuService = QStringLiteral("org.wildos.WindowMenu");
}

System::System(QObject *parent)
    : QObject(parent)
{
}

bool System::canRun(const QString &program) const
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

bool System::run(const QString &program, const QStringList &arguments) const
{
    const QString path = QStandardPaths::findExecutable(program);
    return !path.isEmpty() && QProcess::startDetached(path, arguments);
}

bool System::hasWindowMenu() const
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.interface() && bus.interface()->isServiceRegistered(windowMenuService);
}

void System::showWindowMenu() const
{
    // KWin knows where the pointer is and which window is under it; a
    // Wayland client knows neither.
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        windowMenuService, QStringLiteral("/WindowMenu"), windowMenuService, QStringLiteral("ShowAtPointer")));
}

QString System::passwordHint(const QString &userName) const
{
    // Asked only after a failed attempt, so a short wait is fine; the
    // greeter has no other way to reach AccountsService from QML. An empty
    // name means whoever runs this, as the lock screen does.
    const QString service = QStringLiteral("org.freedesktop.Accounts");
    QDBusMessage find = QDBusMessage::createMethodCall(service, QStringLiteral("/org/freedesktop/Accounts"), service,
                                                       userName.isEmpty() ? QStringLiteral("FindUserById") : QStringLiteral("FindUserByName"));
    if (userName.isEmpty()) {
        find << qint64(::getuid());
    } else {
        find << userName;
    }
    const QDBusReply<QDBusObjectPath> user = QDBusConnection::systemBus().call(find, QDBus::Block, 1000);
    if (!user.isValid()) {
        return {};
    }
    QDBusMessage get = QDBusMessage::createMethodCall(service, user.value().path(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    get << QStringLiteral("org.freedesktop.Accounts.User") << QStringLiteral("PasswordHint");
    const QDBusReply<QDBusVariant> hint = QDBusConnection::systemBus().call(get, QDBus::Block, 1000);
    return hint.isValid() ? hint.value().variant().toString().trimmed() : QString();
}
