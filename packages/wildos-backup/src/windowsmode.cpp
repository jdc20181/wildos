#include "windowsmode.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

WindowsMode WindowsMode::query()
{
    WindowsMode w;
    w.present = QFileInfo::exists(QStringLiteral("/usr/libexec/wildos/winmoded"));
    if (!w.present) {
        return w;
    }
    const QString name = QStringLiteral("org.wildos.WindowsMode1");
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.interface() || !bus.interface()->isServiceRegistered(name).value()) {
        w.state = QStringLiteral("Stopped");
        return w;
    }
    QDBusInterface host(name, QStringLiteral("/org/wildos/WindowsMode1"), QStringLiteral("org.wildos.WindowsMode1.Host"), bus);
    host.setTimeout(5000);
    const QDBusReply<QString> reply = host.call(QStringLiteral("Status"));
    if (!reply.isValid()) {
        w.state = QStringLiteral("Stopped");
        return w;
    }
    const QJsonObject status = QJsonDocument::fromJson(reply.value().toUtf8()).object();
    w.answered = true;
    w.state = status.value(QStringLiteral("state")).toString();
    if (status.value(QStringLiteral("disk_bytes")).isDouble()) {
        w.diskBytes = static_cast<qint64>(status.value(QStringLiteral("disk_bytes")).toDouble());
    }
    return w;
}
