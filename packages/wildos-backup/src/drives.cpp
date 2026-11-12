#include "drives.h"
#include "backupstatus.h"
#include "settings.h"
#include "sizes.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QStorageInfo>
#include <QThread>
#include <QUrl>

using InterfaceProps = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceProps>;
Q_DECLARE_METATYPE(InterfaceProps)
Q_DECLARE_METATYPE(ManagedObjects)

namespace
{
Drives *s_instance = nullptr;

const QString service = QStringLiteral("org.freedesktop.UDisks2");
const QString fsIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");

void registerTypes()
{
    static bool done = false;
    if (!done) {
        qDBusRegisterMetaType<InterfaceProps>();
        qDBusRegisterMetaType<ManagedObjects>();
        done = true;
    }
}

// A stand-in for UDisks2 on test rigs without it: GetManagedObjects as
// JSON, object paths and byte strings written as plain strings.
DriveClass::Objects testObjects(const QString &file)
{
    DriveClass::Objects out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonObject all = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = all.begin(); it != all.end(); ++it) {
        const QJsonObject ifaces = it.value().toObject();
        for (auto i = ifaces.begin(); i != ifaces.end(); ++i) {
            out[it.key()][i.key()] = i.value().toObject().toVariantMap();
        }
    }
    return out;
}
}

QVariantMap DriveInfo::toMap() const
{
    const qint64 free = mountPoint.isEmpty() ? -1 : Drives::freeBytes(mountPoint);
    const qint64 total = mountPoint.isEmpty() ? size : Drives::totalBytes(mountPoint);
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("usable"), usable},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("reasonText"), reasonText},
        {QStringLiteral("canFormat"), canFormat},
        {QStringLiteral("uuid"), uuid},
        {QStringLiteral("name"), name},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("device"), device},
        {QStringLiteral("size"), double(total > 0 ? total : size)},
        {QStringLiteral("free"), double(free)},
        {QStringLiteral("mountPoint"), mountPoint},
        {QStringLiteral("connected"), true},
        {QStringLiteral("known"), !uuid.isEmpty() && !Settings::instance()->drive(uuid).isEmpty()},
    };
}

Drives::Drives(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &Drives::refresh);
    // "known" in the list follows the person's drives.
    connect(Settings::instance(), &Settings::drivesChanged, this, &Drives::changed);
}

Drives *Drives::instance()
{
    if (!s_instance) {
        new Drives(QCoreApplication::instance());
    }
    return s_instance;
}

Drives *Drives::create(QQmlEngine *, QJSEngine *)
{
    Drives *drives = instance();
    QQmlEngine::setObjectOwnership(drives, QQmlEngine::CppOwnership);
    // Only the app judges "too small" against what's included; the
    // scheduled run and the watcher have no use for walking the home folder.
    if (!drives->m_sizes) {
        drives->m_sizes = BackupStatus::instance()->sizes();
        connect(drives->m_sizes, &Sizes::changed, drives, &Drives::changed);
        connect(Settings::instance(), &Settings::includeChanged, drives, &Drives::changed);
        drives->m_sizes->refresh(false);
    }
    drives->watch();
    return drives;
}

QList<DriveInfo> Drives::scan()
{
    registerTypes();
    QDBusInterface manager(service, QStringLiteral("/org/freedesktop/UDisks2"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                           QDBusConnection::systemBus());
    manager.setTimeout(10000);
    QList<DriveInfo> out;
    // Test rigs without UDisks2 (a container, a CI job) stand a folder in
    // for a plugged-in drive.
    const QString testMount = qEnvironmentVariable("WILDOS_BACKUP_TEST_DRIVE");
    if (!testMount.isEmpty()) {
        DriveInfo d;
        d.uuid = QStringLiteral("test-drive");
        d.id = d.uuid;
        d.object = QStringLiteral("/test");
        d.name = QStringLiteral("Test drive");
        d.kind = QCoreApplication::translate("Drives", "USB drive");
        d.mountPoint = testMount;
        d.size = totalBytes(testMount);
        d.shown = true;
        if (QFileInfo(testMount).isDir()) {
            DriveClass::judge(d, 0);
            out.append(d);
        }
    }

    // Everything UDisks2 knows, plugged in before Backup started or since,
    // mounted or not.
    DriveClass::Objects objects;
    const QString fake = qEnvironmentVariable("WILDOS_BACKUP_TEST_UDISKS");
    if (!fake.isEmpty()) {
        objects = testObjects(fake);
    } else {
        const QDBusReply<ManagedObjects> reply = manager.call(QStringLiteral("GetManagedObjects"));
        if (!reply.isValid()) {
            return out;
        }
        const ManagedObjects managed = reply.value();
        for (auto it = managed.cbegin(); it != managed.cend(); ++it) {
            objects.insert(it.key().path(), it.value());
        }
    }
    for (DriveInfo &d : DriveClass::classify(objects)) {
        DriveClass::judge(d, 0);
        out.append(d);
    }
    return out;
}

DriveInfo Drives::find(const QString &uuid)
{
    for (const DriveInfo &d : scan()) {
        if (d.uuid == uuid) {
            return d;
        }
    }
    return {};
}

QString Drives::mount(const DriveInfo &drive, bool interactive, QString *error)
{
    if (!drive.mountPoint.isEmpty()) {
        return drive.mountPoint;
    }
    QDBusInterface fs(service, drive.object, fsIface, QDBusConnection::systemBus());
    // Long enough for someone to type a password at polkit's prompt.
    fs.setTimeout(interactive ? 300000 : 60000);
    const QDBusReply<QString> reply = fs.call(QStringLiteral("Mount"), QVariantMap{{QStringLiteral("auth.no_user_interaction"), !interactive}});
    if (!reply.isValid()) {
        if (error) {
            *error = reply.error().message();
        }
        return {};
    }
    return reply.value();
}

void Drives::mountLater(QObject *owner, const QString &uuid,
                        std::function<void(const QString &mountPoint, const QString &error, const DriveInfo &info)> done)
{
    QPointer<QObject> guard(owner);
    QThread *thread = QThread::create([guard, uuid, done] {
        const DriveInfo info = find(uuid);
        QString error;
        QString mountPoint;
        if (info.uuid.isEmpty()) {
            error = QCoreApplication::translate("Drives", "The drive isn't plugged in any more.");
        } else {
            mountPoint = mount(info, true, &error);
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, done, mountPoint, error, info] {
            if (guard) {
                done(mountPoint, error, info);
            }
        }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString Drives::pluggedIn() const
{
    Settings *settings = Settings::instance();
    const QString preferred = settings->defaultDrive();
    QString found;
    for (const QJsonValue &v : settings->driveList()) {
        const QString uuid = v.toObject().value(QStringLiteral("uuid")).toString();
        for (const DriveInfo &d : m_drives) {
            if (d.uuid == uuid && (found.isEmpty() || uuid == preferred)) {
                found = uuid;
            }
        }
    }
    return found;
}

qint64 Drives::freeBytes(const QString &mountPoint)
{
    QStorageInfo info(mountPoint);
    return info.isValid() ? info.bytesAvailable() : -1;
}

qint64 Drives::totalBytes(const QString &mountPoint)
{
    QStorageInfo info(mountPoint);
    return info.isValid() ? info.bytesTotal() : -1;
}

QVariantList Drives::available() const
{
    QVariantList out;
    const qint64 need = needed();
    for (DriveInfo d : m_drives) {
        DriveClass::judge(d, need);
        if (d.usable) {
            out.append(d.toMap());
        }
    }
    return out;
}

QVariantList Drives::all() const
{
    // Usable ones first, so the choice is at the top of the list.
    QVariantList usable;
    QVariantList others;
    const qint64 need = needed();
    for (DriveInfo d : m_drives) {
        DriveClass::judge(d, need);
        if (d.shown) {
            (d.usable ? usable : others).append(d.toMap());
        }
    }
    return usable + others;
}

qint64 Drives::needed() const
{
    if (!m_sizes) {
        return 0;
    }
    Settings *settings = Settings::instance();
    double total = 0;
    const QList<QPair<bool, double>> parts{
        {settings->includeHome(), m_sizes->home()},
        {settings->includeMail(), m_sizes->mail()},
        {settings->includeSettings(), m_sizes->settings()},
    };
    for (const auto &[on, bytes] : parts) {
        if (on && bytes > 0) {
            total += bytes;
        }
    }
    return qint64(total);
}

void Drives::formatDrive(const QString &device)
{
    // Files gains Format on its own branch; until its --format lands, ask
    // it whether it has one rather than guessing, since an option it
    // doesn't know makes it stop without opening.
    static int hasFormat = -1;
    if (hasFormat < 0) {
        QProcess help;
        help.start(QStringLiteral("wildos-files"), {QStringLiteral("--help")});
        hasFormat = help.waitForFinished(5000) && help.readAllStandardOutput().contains("--format") ? 1 : 0;
    }
    if (hasFormat == 1) {
        QProcess::startDetached(QStringLiteral("wildos-files"), {QStringLiteral("--format"), device});
        return;
    }
    for (const DriveInfo &d : m_drives) {
        if (d.device == device && !d.mountPoint.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(d.mountPoint));
            return;
        }
    }
    // Files lists plugged-in drives, mounted or not, beside Home.
    QProcess::startDetached(QStringLiteral("wildos-files"), {});
}

QVariantMap Drives::mine() const
{
    QVariantMap out;
    for (const QJsonValue &v : Settings::instance()->driveList()) {
        const QString uuid = v.toObject().value(QStringLiteral("uuid")).toString();
        out.insert(uuid, info(uuid));
    }
    return out;
}

QVariantMap Drives::info(const QString &uuid) const
{
    for (const DriveInfo &d : m_drives) {
        if (d.uuid == uuid) {
            return d.toMap();
        }
    }
    const QJsonObject known = Settings::instance()->drive(uuid);
    return {
        {QStringLiteral("uuid"), uuid},
        {QStringLiteral("name"), known.value(QStringLiteral("name")).toString()},
        {QStringLiteral("kind"), known.value(QStringLiteral("kind")).toString()},
        {QStringLiteral("connected"), false},
        {QStringLiteral("size"), -1.0},
        {QStringLiteral("free"), -1.0},
        {QStringLiteral("mountPoint"), QString()},
        {QStringLiteral("known"), !known.isEmpty()},
    };
}

void Drives::refresh()
{
    const QStringList before = [this] {
        QStringList ids;
        for (const DriveInfo &d : m_drives) {
            ids << d.uuid;
        }
        return ids;
    }();
    m_drives = scan();
    for (const DriveInfo &d : m_drives) {
        if (!d.uuid.isEmpty() && !before.contains(d.uuid)) {
            Q_EMIT appeared(d.uuid);
        }
    }
    Q_EMIT changed();
}

void Drives::watch()
{
    if (m_watching) {
        return;
    }
    m_watching = true;
    QDBusConnection bus = QDBusConnection::systemBus();
    const QString om = QStringLiteral("org.freedesktop.DBus.ObjectManager");
    bus.connect(service, QStringLiteral("/org/freedesktop/UDisks2"), om, QStringLiteral("InterfacesAdded"), this, SLOT(somethingChanged()));
    bus.connect(service, QStringLiteral("/org/freedesktop/UDisks2"), om, QStringLiteral("InterfacesRemoved"), this, SLOT(somethingChanged()));
    // Mounting and unmounting change a filesystem's MountPoints.
    bus.connect(service, QString(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), this,
                SLOT(somethingChanged()));
    refresh();
}

void Drives::somethingChanged()
{
    m_debounce.start();
}
