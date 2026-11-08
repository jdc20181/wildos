#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Windows mode (package wildos-winmode): one copy of Windows in the
// background, with every Windows app inside it. What it is doing comes
// from its system service, org.wildos.WindowsMode1 on the system bus, which
// also reports the CPU time and memory of the process Windows runs in: that
// process belongs to the system, and this PC's hardening hides other
// accounts' processes from /proc. The installed apps are the Start entries
// its agent made, listed in ~/.local/share/wildos/winmode/apps.json.
namespace WinMode
{
struct App {
    QString id;
    QString name;
    QString icon;
    // The program inside Windows, which open windows are matched by.
    QString target;
};

struct Status {
    // Whether Windows mode is installed and its service answered.
    bool available = false;
    // running, paused, saved, shut off, or empty when it isn't set up
    QString vm;
    qint64 pid = 0;
    double cpuSeconds = 0;
    qint64 memory = 0;
    // The programs (Windows paths) behind the windows open now.
    QStringList openPrograms;
    // keep, save or automatic, and the wait before it applies
    QString whenIdle;
    int idleSeconds = 60;
};

bool installedOnThisPc();
Status status();
QList<App> installed();
// Which installed app a Windows program belongs to, or empty.
QString appFor(const QList<App> &apps, const QString &program);

// Plain words for the board: Running, Paused, Saved to disk, Off.
QString stateText(const QString &vm);
// What happens when no Windows app is open, as configured.
QString idleText(const QString &whenIdle, int seconds);
}
