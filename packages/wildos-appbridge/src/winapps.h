#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Windows apps installed by earlier versions of App Bridge, which ran them
// with Wine in a sandbox of their own. Wild OS no longer has Wine: every
// Windows program runs in Windows mode. What is
// left here lets Settings list those apps' folders and remove them, and
// takes their Start entries away, since nothing can start them now. Their
// folders, ~/.local/share/wildos/windows-apps/<id>/, are kept until the
// person removes them, as they may hold the apps' own files.
//
// ~/.config/wildos-appbridgerc [General] holds Enabled (Windows programs on
// or off) and StartingPermissions, which Settings still writes.
namespace WinApps
{
struct Permissions {
    bool documents = true;
    bool printer = true;
    bool internet = false;
};

QString root();
QString appDir(const QString &id);
QString makeId(const QString &name);

bool supportEnabled();
Permissions startingPermissions();

QJsonObject load(const QString &id);
QJsonArray list();
bool remove(const QString &id);
// Removes the Start entries of apps installed with Wine. Returns how many.
int tidy();
}
