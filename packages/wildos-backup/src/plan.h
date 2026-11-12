#pragma once

#include <QStringList>

class Settings;

// What a backup takes and leaves, from the What's included switches.
//
//   Your home folder         everything under home but the two below
//   Mail, Calendar, ...      Evolution's data and settings, and Mail's own
//   App settings and themes  ~/.config and ~/.local/share
//
// Always left out: Downloads, caches (~/.cache and any folder with a
// CACHEDIR.TAG), the bin, Flatpak runtimes, and folders the person
// excluded.
struct Plan {
    QStringList paths;
    // restic exclude patterns, in order: a pattern starting with ! puts
    // back something a pattern before it took out.
    QStringList excludes;

    static Plan from(const Settings *settings);

    // The pieces, also used to work out the sizes on the Overview.
    static QStringList mailPaths();
    static QStringList settingsPaths();
    // Real folders to leave out, before the person's own.
    static QStringList alwaysOut();
    static QString downloads();
    // restic patterns treat * ? [ and \ as special; a folder's own name may
    // hold them.
    static QString escape(const QString &path);
};
