#include "options.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace Paths
{
QString configFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/wildos/windows-mode.json");
}
QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/wildos/windows-mode");
}
QString runtimeDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/wildos-windows-mode");
}
QString mediaDir() { return dataDir() + QStringLiteral("/media"); }
QString disk() { return dataDir() + QStringLiteral("/windows.qcow2"); }
QString nvram() { return dataDir() + QStringLiteral("/nvram.fd"); }
QString msdm() { return dataDir() + QStringLiteral("/msdm.bin"); }
QString credentials() { return dataDir() + QStringLiteral("/credentials"); }
QString setupState() { return dataDir() + QStringLiteral("/setup.json"); }
QString unattendIso() { return dataDir() + QStringLiteral("/unattend.iso"); }
QString agentSocket() { return runtimeDir() + QStringLiteral("/agent.sock"); }
QString spiceSocket() { return runtimeDir() + QStringLiteral("/spice.sock"); }
QString statusFile() { return runtimeDir() + QStringLiteral("/status.json"); }
QString applicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
}
QString iconsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/icons/hicolor/256x256/apps");
}
}

QJsonObject Options::toJson() const
{
    return {
        {QStringLiteral("office"), office},
        {QStringLiteral("officePlan"), officePlan},
        {QStringLiteral("openFiles"), openFiles},
        {QStringLiteral("whenIdle"), whenIdle},
        {QStringLiteral("memoryGb"), memoryGb},
        {QStringLiteral("folders"), QJsonArray::fromStringList(folders)},
        {QStringLiteral("devices"), devices},
    };
}

Options Options::fromJson(const QJsonObject &json)
{
    Options o;
    o.office = json.value(QStringLiteral("office")).toBool(o.office);
    o.officePlan = json.value(QStringLiteral("officePlan")).toString(o.officePlan);
    o.openFiles = json.value(QStringLiteral("openFiles")).toBool(o.openFiles);
    o.whenIdle = json.value(QStringLiteral("whenIdle")).toString(o.whenIdle);
    o.memoryGb = qBound(4, json.value(QStringLiteral("memoryGb")).toInt(o.memoryGb), 64);
    if (json.contains(QStringLiteral("folders"))) {
        o.folders.clear();
        for (const QJsonValue &v : json.value(QStringLiteral("folders")).toArray()) {
            if (Folders::all().contains(v.toString())) {
                o.folders << v.toString();
            }
        }
    }
    o.devices = json.value(QStringLiteral("devices")).toString(o.devices);
    return o;
}

Options Options::load()
{
    QFile file(Paths::configFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return fromJson(QJsonDocument::fromJson(file.readAll()).object());
}

bool Options::save() const
{
    QDir().mkpath(QFileInfo(Paths::configFile()).path());
    QSaveFile file(Paths::configFile());
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson());
    return file.commit();
}

namespace Folders
{
QStringList all()
{
    return {QStringLiteral("Documents"), QStringLiteral("Downloads"), QStringLiteral("Pictures"), QStringLiteral("Desktop"), QStringLiteral("Music"),
            QStringLiteral("Videos")};
}

QString hostPath(const QString &name)
{
    static const QHash<QString, QStandardPaths::StandardLocation> where{
        {QStringLiteral("Documents"), QStandardPaths::DocumentsLocation}, {QStringLiteral("Downloads"), QStandardPaths::DownloadLocation},
        {QStringLiteral("Pictures"), QStandardPaths::PicturesLocation},   {QStringLiteral("Desktop"), QStandardPaths::DesktopLocation},
        {QStringLiteral("Music"), QStandardPaths::MusicLocation},         {QStringLiteral("Videos"), QStandardPaths::MoviesLocation},
    };
    return where.contains(name) ? QStandardPaths::writableLocation(where.value(name)) : QString();
}

QString driveLetter(const QString &name)
{
    static const QHash<QString, QString> letters{
        {QStringLiteral("Documents"), QStringLiteral("Z")}, {QStringLiteral("Downloads"), QStringLiteral("Y")}, {QStringLiteral("Pictures"), QStringLiteral("X")},
        {QStringLiteral("Desktop"), QStringLiteral("W")},   {QStringLiteral("Music"), QStringLiteral("V")},     {QStringLiteral("Videos"), QStringLiteral("U")},
    };
    return letters.value(name);
}
}
