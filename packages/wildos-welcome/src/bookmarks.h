#pragma once

#include <QList>
#include <QString>

struct BrowserProfile {
    QString browser;
    QString path;
};

// Edge, Chrome and Brave all keep bookmarks in the same Chromium JSON file,
// so Windows bookmarks can go straight into Brave's own file as a folder
// per browser, with fresh ids so nothing clashes.
namespace Bookmarks
{
QList<BrowserProfile> findWindowsProfiles(const QString &windowsHome);
bool braveInstalled();
bool braveRunning();
QString braveBookmarksFile();

// Adds a "From Edge" or "From Chrome" folder to Brave's bookmarks bar for
// each browser found. Returns the number of bookmarks added, or -1 if
// Brave's file could not be written. Brave must not be running, or it
// would write its own copy over ours when it closes.
int importInto(const QString &braveFile, const QList<BrowserProfile> &profiles);
}

// Browsing history comes across too: Edge and Chrome keep it in the same
// Chromium SQLite file as Brave, beside the bookmarks.
namespace History
{
QString braveHistoryFile();

// Creates Brave's profile by starting Brave once without a window, for when
// Brave has never been opened and so has no history file to add to.
bool createBraveProfile();

// Adds every web page from each profile's history that Brave doesn't
// already have, with its visits. Returns the number of pages added, or -1
// if Brave's history could not be written. Brave must not be running: it
// keeps the file locked.
int importInto(const QString &braveHistory, const QList<BrowserProfile> &profiles);
}
