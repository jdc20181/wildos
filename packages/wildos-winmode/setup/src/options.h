#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Where Windows mode keeps things, and the choices from 9.4 and 9.8.
//
//   ~/.config/wildos/windows-mode.json          the Options below
//   ~/.local/share/wildos/windows-mode/         the VM
//   $XDG_RUNTIME_DIR/wildos-windows-mode/       sockets, and status.json for
//                                                Task Manager
namespace Paths
{
QString configFile();
QString dataDir();
QString runtimeDir();
QString mediaDir();
QString disk();
QString nvram();
QString msdm();
QString credentials();
QString setupState();
QString unattendIso();
QString agentSocket();
QString spiceSocket();
QString statusFile();
QString applicationsDir();
QString iconsDir();
}

struct Options {
    bool office = true;
    // personal, business or work
    QString officePlan = QStringLiteral("personal");
    bool openFiles = true;
    // automatic, memory, disk or shutdown
    QString whenIdle = QStringLiteral("automatic");
    int memoryGb = 4;
    QStringList folders = {QStringLiteral("Documents"), QStringLiteral("Downloads"), QStringLiteral("Pictures"), QStringLiteral("Desktop")};
    // ask, always or never
    QString devices = QStringLiteral("ask");

    QJsonObject toJson() const;
    static Options fromJson(const QJsonObject &json);
    static Options load();
    bool save() const;
};

namespace Folders
{
// The folders Windows apps can be given, in the order Settings lists them.
QStringList all();
// The person's real folder for one of them, from the XDG user dirs.
QString hostPath(const QString &name);
// The drive letter each shared folder has inside Windows; fixed, so a
// path saved in a Windows app's recent list stays right.
QString driveLetter(const QString &name);
}
