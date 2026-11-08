#include "startupapps.h"

#include <KConfigGroup>
#include <KDesktopFile>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QStandardPaths>

#include <algorithm>

namespace
{
QString userDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
}

QStringList systemDirs()
{
    QStringList dirs;
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_DIRS", QStringLiteral("/etc/xdg"));
    for (const QString &dir : xdg.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        dirs << dir + QStringLiteral("/autostart");
    }
    return dirs;
}

// Parts of the desktop that start themselves at sign-in. They are not apps
// a person chose, and turning one off breaks the desktop, so they stay
// off the list.
bool isDesktopPart(const QString &file, const KConfigGroup &entry)
{
    static const QStringList prefixes = {
        QStringLiteral("org.kde."), QStringLiteral("at-spi"), QStringLiteral("xdg-user-dirs"), QStringLiteral("pulseaudio"),
        QStringLiteral("pipewire"), QStringLiteral("geoclue"), QStringLiteral("polkit"), QStringLiteral("gnome-keyring"),
        QStringLiteral("wildos"), QStringLiteral("org.wildos."), QStringLiteral("kaccess"), QStringLiteral("baloo"),
        QStringLiteral("gmenudbusmenuproxy"), QStringLiteral("xembedsniproxy"), QStringLiteral("pam_kwallet"), QStringLiteral("powerdevil"),
        QStringLiteral("kde"), QStringLiteral("spice-vdagent"), QStringLiteral("snap-userd"), QStringLiteral("print-applet"),
    };
    for (const QString &prefix : prefixes) {
        if (file.startsWith(prefix)) {
            return true;
        }
    }
    const QStringList only = entry.readXdgListEntry("OnlyShowIn");
    return entry.readEntry("NoDisplay", false) || only.contains(QStringLiteral("KDE"));
}
}

StartupApps::StartupApps(QObject *parent)
    : QObject(parent)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(250);
    connect(&m_settle, &QTimer::timeout, this, &StartupApps::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_settle, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_settle, qOverload<>(&QTimer::start));
    reload();
}

void StartupApps::watch()
{
    QDir().mkpath(userDir());
    const QStringList dirs = QStringList{userDir()} + systemDirs();
    for (const QString &dir : dirs) {
        if (QFileInfo::exists(dir) && !m_watcher.directories().contains(dir)) {
            m_watcher.addPath(dir);
        }
    }
    // Hidden= is written into an existing file, which the folder does not
    // report.
    const auto own = QDir(userDir()).entryList({QStringLiteral("*.desktop")}, QDir::Files);
    for (const QString &name : own) {
        const QString path = userDir() + QLatin1Char('/') + name;
        if (!m_watcher.files().contains(path)) {
            m_watcher.addPath(path);
        }
    }
}

void StartupApps::reload()
{
    // The first directory to hold a file wins, the user's own first.
    QMap<QString, QString> files;
    const QStringList dirs = QStringList{userDir()} + systemDirs();
    for (const QString &dir : dirs) {
        const auto entries = QDir(dir).entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &name : entries) {
            if (!files.contains(name)) {
                files.insert(name, dir + QLatin1Char('/') + name);
            }
        }
    }

    QVariantList apps;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        KDesktopFile desktop(it.value());
        const KConfigGroup entry = desktop.desktopGroup();
        if (isDesktopPart(it.key(), entry) || entry.readEntry("Exec").isEmpty() || desktop.readName().isEmpty()) {
            continue;
        }
        const bool on = !entry.readEntry("Hidden", false) && entry.readEntry("X-GNOME-Autostart-enabled", true);
        apps.append(QVariantMap{
            {QStringLiteral("file"), it.key()},
            {QStringLiteral("name"), desktop.readName()},
            {QStringLiteral("description"), desktop.readComment()},
            {QStringLiteral("icon"), desktop.readIcon()},
            {QStringLiteral("enabled"), on},
        });
    }
    std::sort(apps.begin(), apps.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    m_apps = apps;
    watch();
    Q_EMIT appsChanged();
}

void StartupApps::setEnabled(const QString &file, bool on)
{
    if (file.contains(QLatin1Char('/'))) {
        return;
    }
    const QString own = userDir() + QLatin1Char('/') + file;
    QString shipped;
    for (const QString &dir : systemDirs()) {
        if (QFile::exists(dir + QLatin1Char('/') + file)) {
            shipped = dir + QLatin1Char('/') + file;
            break;
        }
    }

    if (on && !shipped.isEmpty() && QFile::exists(own)) {
        // Our override only switched the shipped entry off; dropping it
        // brings the app's own file back exactly as it was.
        KDesktopFile override(own);
        if (override.desktopGroup().readEntry("Hidden", false)) {
            QFile::remove(own);
            reload();
            return;
        }
    }
    if (!QFile::exists(own)) {
        if (shipped.isEmpty()) {
            return;
        }
        QDir().mkpath(userDir());
        QFile::copy(shipped, own);
        QFile(own).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
    KDesktopFile desktop(own);
    KConfigGroup entry = desktop.desktopGroup();
    entry.writeEntry("Hidden", !on);
    if (entry.hasKey("X-GNOME-Autostart-enabled")) {
        entry.writeEntry("X-GNOME-Autostart-enabled", on);
    }
    desktop.sync();
    reload();
}
