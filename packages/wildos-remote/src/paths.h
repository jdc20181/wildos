#pragma once

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>

// Where Remote keeps things, all of it on this PC: settings and hosts in
// ~/.config/wildos-remote, snippets and history in ~/.local/share, screens
// in the cache, sockets in the runtime folder, and keys where ssh expects
// them.
namespace Paths
{
inline QString config()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/wildos-remote");
}

inline QString data()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/wildos-remote");
}

inline QString cache()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/wildos-remote");
}

// Only this user may enter it: the askpass and connection-sharing sockets
// live here.
inline QString runtime()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    const QString dir = base + QStringLiteral("/wildos-remote");
    QDir().mkpath(dir);
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return dir;
}

// ~/.ssh, made the way ssh insists on: only its owner may enter.
inline QString ssh()
{
    const QString dir = QDir::homePath() + QStringLiteral("/.ssh");
    if (!QDir(dir).exists()) {
        QDir().mkpath(dir);
        QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    return dir;
}
}
