#include "appsmodel.h"

#include <KApplicationTrader>
#include <KService>

#include <PackageKit/Daemon>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

using PackageKit::Daemon;
using PackageKit::Transaction;

namespace
{
// Packages that make up the desktop itself. Removing any of them would take
// Wild OS apart, whatever the dependency tree allows.
const QStringList coreMetapackages = {QStringLiteral("wildos-desktop"), QStringLiteral("wildos-base"), QStringLiteral("wildos-branding")};

// The Store and Settings are how people add, remove and fix everything else,
// so they stay even though Wild OS only recommends the Store.
const QStringList managementPackages = {QStringLiteral("plasma-discover"), QStringLiteral("systemsettings"), QStringLiteral("wildos-settings"),
                                        QStringLiteral("kinfocenter")};

QStringList dependsOf(const QString &status, const QString &package)
{
    const QRegularExpression block(QStringLiteral("(?:^|\\n)Package: %1\\n(.*?)(?:\\n\\n|$)").arg(QRegularExpression::escape(package)),
                                   QRegularExpression::DotMatchesEverythingOption);
    const auto match = block.match(status);
    if (!match.hasMatch()) {
        return {};
    }
    QStringList names;
    const auto lines = match.captured(1).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("Depends:")) || line.startsWith(QLatin1String("Pre-Depends:"))) {
            const auto items = line.section(QLatin1Char(':'), 1).split(QRegularExpression(QStringLiteral("[,|]")));
            for (const QString &item : items) {
                const QString name = item.trimmed().section(QLatin1Char(' '), 0, 0).section(QLatin1Char(':'), 0, 0);
                if (!name.isEmpty()) {
                    names << name;
                }
            }
        }
    }
    return names;
}
}

AppsModel::AppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Lets polkit ask for the password. Without this hint PackageKit
    // marks every request as non-interactive and polkit refuses at once,
    // so installs failed with "needs the administrator password" and no
    // prompt ever appeared.
    PackageKit::Daemon::global()->setHints(QStringList{QStringLiteral("interactive=true")});
    reload();
}

QHash<QString, QString> AppsModel::desktopOwners() const
{
    QHash<QString, QString> owners;
    QDir info(QStringLiteral("/var/lib/dpkg/info"));
    const auto lists = info.entryList({QStringLiteral("*.list")}, QDir::Files);
    for (const QString &list : lists) {
        QFile file(info.filePath(list));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString package = list.section(QLatin1Char('.'), 0, -2).section(QLatin1Char(':'), 0, 0);
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.startsWith("/usr/share/applications/") && line.endsWith(".desktop")) {
                owners.insert(QFileInfo(QString::fromUtf8(line)).fileName(), package);
            }
        }
    }
    return owners;
}

QSet<QString> AppsModel::protectedPackages() const
{
    QFile file(QStringLiteral("/var/lib/dpkg/status"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QString status = QString::fromUtf8(file.readAll());
    QSet<QString> names(coreMetapackages.begin(), coreMetapackages.end());
    names.unite(QSet<QString>(managementPackages.begin(), managementPackages.end()));
    for (const QString &meta : coreMetapackages) {
        const auto deps = dependsOf(status, meta);
        names.unite(QSet<QString>(deps.begin(), deps.end()));
    }
    return names;
}

void AppsModel::reload()
{
    const QHash<QString, QString> owners = desktopOwners();
    const QSet<QString> core = protectedPackages();

    QList<App> apps;
    QSet<QString> seen;
    const KService::List services = KApplicationTrader::query([](const KService::Ptr &service) {
        return !service->noDisplay() && service->showOnCurrentPlatform();
    });
    for (const KService::Ptr &service : services) {
        const QString id = service->desktopEntryName();
        if (seen.contains(id) || service->name().isEmpty()) {
            continue;
        }
        seen.insert(id);
        App app;
        app.name = service->name();
        app.icon = service->icon();
        app.id = id;
        app.flatpak = service->property<QString>(QStringLiteral("X-Flatpak"));
        app.userFlatpak = service->entryPath().contains(QLatin1String("/.local/share/flatpak/"));
        app.package = owners.value(QFileInfo(service->entryPath()).fileName());
        app.protectedApp = app.flatpak.isEmpty() && (app.package.isEmpty() || core.contains(app.package) || app.package.startsWith(QLatin1String("wildos-")));
        apps << app;
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(apps.begin(), apps.end(), [&collator](const App &a, const App &b) {
        return collator.compare(a.name, b.name) < 0;
    });

    m_all = apps;
    applyFilter();
}

void AppsModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    applyFilter();
}

void AppsModel::applyFilter()
{
    beginResetModel();
    m_shown.clear();
    const QString needle = m_filter.trimmed();
    for (const App &app : std::as_const(m_all)) {
        if (needle.isEmpty() || app.name.contains(needle, Qt::CaseInsensitive)) {
            m_shown << app;
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

int AppsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_shown.size());
}

QVariant AppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size()) {
        return {};
    }
    const App &app = m_shown.at(index.row());
    switch (role) {
    case NameRole:
        return app.name;
    case IconRole:
        return app.icon;
    case IdRole:
        return app.id;
    case SourceRole:
        return app.flatpak.isEmpty() ? QStringLiteral("system") : QStringLiteral("store");
    case PackageRole:
        return app.flatpak.isEmpty() ? app.package : app.flatpak;
    case CanUninstallRole:
        return !app.protectedApp;
    }
    return {};
}

QHash<int, QByteArray> AppsModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IconRole, "appIcon"},
        {IdRole, "appId"},
        {SourceRole, "source"},
        {PackageRole, "packageName"},
        {CanUninstallRole, "canUninstall"},
    };
}

void AppsModel::uninstall(const QString &id)
{
    if (!m_busyId.isEmpty()) {
        return;
    }
    auto it = std::find_if(m_all.cbegin(), m_all.cend(), [&id](const App &app) {
        return app.id == id;
    });
    if (it == m_all.cend() || it->protectedApp) {
        return;
    }
    const App app = *it;
    m_busyId = id;
    Q_EMIT busyChanged();

    if (!app.flatpak.isEmpty()) {
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, app](int code, QProcess::ExitStatus) {
            process->deleteLater();
            finishUninstall(app, code == 0, QString::fromUtf8(process->readAllStandardError()).trimmed());
        });
        QStringList args = {QStringLiteral("uninstall"), QStringLiteral("--noninteractive"), QStringLiteral("-y")};
        args << (app.userFlatpak ? QStringLiteral("--user") : QStringLiteral("--system")) << app.flatpak;
        process->start(QStringLiteral("flatpak"), args);
        return;
    }

    Transaction *resolve = Daemon::resolve(app.package, Transaction::FilterInstalled);
    auto packageId = std::make_shared<QString>();
    connect(resolve, &Transaction::package, this, [packageId](Transaction::Info, const QString &pid, const QString &) {
        *packageId = pid;
    });
    connect(resolve, &Transaction::finished, this, [this, packageId, app](Transaction::Exit status, uint) {
        if (status != Transaction::ExitSuccess || packageId->isEmpty()) {
            finishUninstall(app, false, QString());
            return;
        }
        // allowDeps stays off: if anything else needs this app, PackageKit
        // refuses rather than quietly taking other apps with it.
        Transaction *remove = Daemon::removePackages({*packageId}, false, true);
        auto reason = std::make_shared<QString>();
        connect(remove, &Transaction::errorCode, this, [reason](Transaction::Error error, const QString &) {
            *reason = error == Transaction::ErrorDepResolutionFailed || error == Transaction::ErrorCannotRemoveSystemPackage
                ? QStringLiteral("Other apps need it, so it was left in place.")
                : error == Transaction::ErrorNotAuthorized ? QStringLiteral("It was not removed because the password was not given.") : QString();
        });
        connect(remove, &Transaction::finished, this, [this, app, reason](Transaction::Exit status, uint) {
            finishUninstall(app, status == Transaction::ExitSuccess, *reason);
        });
    });
}

void AppsModel::finishUninstall(const App &app, bool ok, const QString &reason)
{
    m_busyId.clear();
    Q_EMIT busyChanged();
    if (ok) {
        Q_EMIT uninstalled(app.name);
        reload();
    } else {
        Q_EMIT uninstallFailed(app.name, reason);
    }
}
