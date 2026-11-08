#include "winapps.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
const QString settingsFile = QStringLiteral("wildos-appbridgerc");

QString applicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}
}

namespace WinApps
{
QString root()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/wildos/windows-apps");
}

QString appDir(const QString &id)
{
    return root() + QLatin1Char('/') + id;
}

QString makeId(const QString &name)
{
    QString id = name.toLower();
    id.replace(QLatin1Char('+'), QStringLiteral("-plus"));
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (id.isEmpty()) {
        id = QStringLiteral("windows-app");
    }
    return id.left(48);
}

bool supportEnabled()
{
    return KSharedConfig::openConfig(settingsFile)->group(QStringLiteral("General")).readEntry("Enabled", true);
}

Permissions startingPermissions()
{
    const QString choice = KSharedConfig::openConfig(settingsFile)->group(QStringLiteral("General")).readEntry("StartingPermissions", QStringLiteral("documents-offline"));
    Permissions p;
    p.documents = choice != QLatin1String("own-offline");
    p.internet = choice == QLatin1String("documents-online");
    p.printer = true;
    return p;
}

QJsonObject load(const QString &id)
{
    const QString path = appDir(id) + QStringLiteral("/app.conf");
    if (id.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }
    KConfig config(path, KConfig::SimpleConfig);
    const KConfigGroup app = config.group(QStringLiteral("App"));
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), app.readEntry("Name", id)},
        {QStringLiteral("icon"), app.readEntry("Icon")},
        {QStringLiteral("publisher"), app.readEntry("Publisher")},
        {QStringLiteral("installedFrom"), app.readEntry("InstalledFrom")},
        {QStringLiteral("installed"), app.readEntry("Installed")},
        {QStringLiteral("launchers"), QJsonArray::fromStringList(app.readEntry("Launchers", QStringList()))},
    };
}

QJsonArray list()
{
    QJsonArray apps;
    const auto ids = QDir(root()).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &id : ids) {
        const QJsonObject app = load(id);
        if (!app.isEmpty()) {
            apps.append(app);
        }
    }
    return apps;
}

bool remove(const QString &id)
{
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.startsWith(QLatin1Char('.'))) {
        return false;
    }
    const QJsonArray launchers = load(id).value(QStringLiteral("launchers")).toArray();
    for (const QJsonValue &launcher : launchers) {
        QFile::remove(applicationsDir() + QLatin1Char('/') + QFileInfo(launcher.toString()).fileName());
    }
    return QDir(appDir(id)).removeRecursively();
}

int tidy()
{
    int removed = 0;
    const QDir applications(applicationsDir());
    const auto files = applications.entryList({QStringLiteral("wildos-winapp-*.desktop")}, QDir::Files);
    for (const QString &name : files) {
        if (QFile::remove(applications.filePath(name))) {
            ++removed;
        }
    }
    return removed;
}
}
