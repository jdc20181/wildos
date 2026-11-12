#include "plan.h"
#include "paths.h"
#include "settings.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
QString inHome(const char *relative)
{
    return Paths::home() + QLatin1Char('/') + QLatin1String(relative);
}
}

QStringList Plan::mailPaths()
{
    return {
        inHome(".local/share/evolution"),
        inHome(".config/evolution"),
        inHome(".local/share/wildos-mail"),
        inHome(".config/wildos-mail"),
    };
}

QStringList Plan::settingsPaths()
{
    return {inHome(".config"), inHome(".local/share")};
}

QString Plan::downloads()
{
    const QString d = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    return d == QDir::cleanPath(Paths::home()) ? inHome("Downloads") : d;
}

QStringList Plan::alwaysOut()
{
    return {
        downloads(),
        inHome(".cache"),
        inHome(".local/share/Trash"),
        inHome(".local/share/flatpak"),
        // The file search index, rebuilt on its own.
        inHome(".local/share/baloo"),
        // Windows mode's setup media; the Windows disk has its own switch.
        inHome(".local/share/wildos/windows-mode"),
    };
}

QString Plan::escape(const QString &path)
{
    QString out;
    out.reserve(path.size());
    for (const QChar c : path) {
        if (c == QLatin1Char('*') || c == QLatin1Char('?') || c == QLatin1Char('[') || c == QLatin1Char(']') || c == QLatin1Char('\\')) {
            out.append(QLatin1Char('\\'));
        }
        out.append(c);
    }
    return out;
}

Plan Plan::from(const Settings *settings)
{
    Plan plan;
    const bool home = settings->includeHome();
    const bool mail = settings->includeMail();
    const bool appSettings = settings->includeSettings();

    if (home) {
        plan.paths << Paths::home();
        if (!appSettings) {
            // Everything inside the two settings folders goes, then Mail's
            // folders come back if Mail is on. restic can only put back
            // what sits inside a folder it still visits, hence /* rather
            // than the folder itself.
            for (const QString &p : settingsPaths()) {
                plan.excludes << escape(p) + QStringLiteral("/*");
            }
            if (mail) {
                for (const QString &p : mailPaths()) {
                    plan.excludes << QLatin1Char('!') + escape(p);
                }
            }
        }
    } else {
        if (appSettings) {
            plan.paths << settingsPaths();
        }
        if (mail && !appSettings) {
            plan.paths << mailPaths();
        }
    }
    if (!mail) {
        for (const QString &p : mailPaths()) {
            plan.excludes << escape(p);
        }
    }

    for (const QString &p : alwaysOut()) {
        plan.excludes << escape(p);
    }
    // Each Flatpak app's own cache.
    plan.excludes << escape(inHome(".var/app")) + QStringLiteral("/*/cache");
    for (const QString &p : settings->excludes()) {
        plan.excludes << escape(p);
    }

    // Only folders that exist: restic stops at a path it cannot find.
    QStringList present;
    for (const QString &p : std::as_const(plan.paths)) {
        if (QFileInfo::exists(p)) {
            present << p;
        }
    }
    plan.paths = present;
    return plan;
}
