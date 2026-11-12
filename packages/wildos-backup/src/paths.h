#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Where Backup keeps things. The app and the scheduled run (wildos-backup
// --run) are separate processes, so both find each other through these.
namespace Paths
{
// The folder made on each backup drive for its restic repository.
inline QString repoFolder()
{
    return QStringLiteral("WildOS Backup");
}

inline QString home()
{
    return QDir::homePath();
}

// Per sign-in, in tmpfs: the running backup's progress and its lock.
inline QString runtimeDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    return base + QStringLiteral("/wildos-backup");
}
inline QString statusFile()
{
    return runtimeDir() + QStringLiteral("/status.json");
}
inline QString lockFile()
{
    return runtimeDir() + QStringLiteral("/run.lock");
}

// What happened, kept across restarts: the list of runs and a note of a
// missed run waiting for the drive.
inline QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/wildos-backup");
}
inline QString historyFile()
{
    return stateDir() + QStringLiteral("/history.json");
}
inline QString pendingFile()
{
    return stateDir() + QStringLiteral("/pending.json");
}

inline QString settingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/wildos/backup.conf");
}

inline QString sizesCache()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/wildos-backup/sizes.json");
}

// The schedule the app sets, as a drop-in over the packaged timer.
inline QString timerDropIn()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/systemd/user/wildos-backup.timer.d/schedule.conf");
}

inline QString windowsHelper()
{
    return QStringLiteral("/usr/libexec/wildos-backup/windows-disk");
}

// Makes the repository folder on a drive whose top folder is root's.
inline QString folderHelper()
{
    return QStringLiteral("/usr/libexec/wildos-backup/make-folder");
}

inline const char *keyringService()
{
    return "Wild OS Backup";
}
inline const char *keyringKey()
{
    return "repository password";
}
}
