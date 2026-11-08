#pragma once

#include <QHash>
#include <QString>

// Works out who is asking for permission and whether Wild OS can vouch for
// it, for the permission prompt.
//
// The app is the process polkit names as the subject: its systemd scope
// says which app the desktop started (app-<desktop id>...), its executable
// says which package it came from. For pkexec the program being run as
// administrator matters more than the shell that asked, so that is what
// is checked.
//
// Verified means every file involved (the action's policy, the program
// and the asking app) belongs to an installed package, is unchanged since
// it was installed (dpkg's checksums), and its installed version either
// shipped in the image (/usr/share/wildos/image-packages) or comes, by
// apt, from Debian, Debian security, Kicksecure or the Wild OS repository.
// Anything else is unverified: a script in a home folder, a Flatpak, a
// package from another vendor or installed by hand.
struct Verdict {
    QString appName;
    QString iconName;
    QString purpose;
    bool verified = false;
    QString why;
    QString program;
    QString commandLine;
    QString caller;
    QString package;
    QString origin;
    QString vendor;
};

namespace Verifier
{
// details: the polkit details (keys polkit.subject-pid, polkit.caller-pid,
// program, command_line). message is polkit's own sentence, the fallback
// when Wild OS has no plain wording of its own for the action.
Verdict check(const QString &actionId, const QString &message, const QHash<QString, QString> &details);

// One plain line for an action, or empty when Wild OS has none.
QString purposeOf(const QString &actionId, const QString &program);
}
