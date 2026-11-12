#pragma once

#include <QString>

// What Backup needs from Windows mode (wildos-winmode): is it set up, is
// Windows shut down, and how big its disk is. The disk itself is root's
// (/var/lib/wildos/winmode/windows.qcow2), so the backup reads it through
// /usr/libexec/wildos-backup/windows-disk, which checks again as root that
// Windows is shut down before and after.
struct WindowsMode {
    // The package is there at all.
    bool present = false;
    // Its service answered; when it isn't running, Windows can't be either.
    bool answered = false;
    // "NotInstalled", "Stopped", "Running", "Paused", "Saved", ...
    QString state;
    qint64 diskBytes = -1;

    bool setUp() const
    {
        return present && state != QLatin1String("NotInstalled");
    }
    bool shutDown() const
    {
        return present && (!answered || state == QLatin1String("Stopped"));
    }

    // Asks the running service only; never starts it.
    static WindowsMode query();
};
