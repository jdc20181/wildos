#include "drives.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QStorageInfo>
#include <QThread>

using InterfaceProps = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceProps>;
Q_DECLARE_METATYPE(InterfaceProps)
Q_DECLARE_METATYPE(ManagedObjects)

namespace
{
Drives *s_instance = nullptr;

const QString service = QStringLiteral("org.freedesktop.UDisks2");
const QString blockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString fsIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString driveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");

void registerTypes()
{
    static bool done = false;
    if (!done) {
        qDBusRegisterMetaType<InterfaceProps>();
        qDBusRegisterMetaType<ManagedObjects>();
        done = true;
    }
}

// UDisks2 sends paths as NUL-terminated byte arrays.
QString bytes(const QVariant &v)
{
    return QString::fromUtf8(v.toByteArray().constData());
}

QStringList byteArrays(const QVariant &v)
{
    QStringList out;
    if (v.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QByteArray b;
            arg >> b;
            out << QString::fromUtf8(b.constData());
        }
        arg.endArray();
    }
    return out;
}

bool systemMount(const QString &mountPoint)
{
    static const QStringList system{
        QStringLiteral("/"), QStringLiteral("/home"), QStringLiteral("/boot"), QStringLiteral("/boot/efi"),
        QStringLiteral("/efi"), QStringLiteral("/usr"), QStringLiteral("/var"),
    };
    return system.contains(mountPoint);
}

// Filesystems a backup can be written to.
bool writableType(const QString &type)
{
    static const QStringList ok{
        QStringLiteral("vfat"), QStringLiteral("exfat"), QStringLiteral("ntfs"), QStringLiteral("ntfs3"),
        QStringLiteral("ext2"), QStringLiteral("ext3"), QStringLiteral("ext4"), QStringLiteral("btrfs"),
        QStringLiteral("xfs"), QStringLiteral("f2fs"), QStringLiteral("hfsplus"),
    };
    return ok.contains(type);
}
}

QVariantMap DriveInfo::toMap() const
{
    const qint64 free = mountPoint.isEmpty() ? -1 : Drives::freeBytes(mountPoint);
    const qint64 total = mountPoint.isEmpty() ? size : Drives::totalBytes(mountPoint);
    return {
        {QStringLiteral("uuid"), uuid},
        {QStringLiteral("name"), name},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("device"), device},
        {QStringLiteral("size"), double(total > 0 ? total : size)},
        {QStringLiteral("free"), double(free)},
        {QStringLiteral("mountPoint"), mountPoint},
        {QStringLiteral("connected"), true},
        {QStringLiteral("known"), !Settings::instance()->drive(uuid).isEmpty()},
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
    drives->watch();
    return drives;
}

QList<DriveInfo> Drives::scan()
{
    registerTypes();
    QDBusInterface manager(service, QStringLiteral("/org/freedesktop/UDisks2"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                           QDBusConnection::systemBus());
    manager.setTimeout(10000);
    const QDBusReply<ManagedObjects> reply = manager.call(QStringLiteral("GetManagedObjects"));
    QList<DriveInfo> out;
    // Test rigs without UDisks2 (a container, a CI job) stand a folder in
    // for a plugged-in drive.
    const QString testMount = qEnvironmentVariable("WILDOS_BACKUP_TEST_DRIVE");
    if (!testMount.isEmpty()) {
        DriveInfo d;
        d.uuid = QStringLiteral("test-drive");
        d.object = QStringLiteral("/test");
        d.name = QStringLiteral("Test drive");
        d.kind = QCoreApplication::translate("Drives", "USB drive");
        d.mountPoint = testMount;
        d.size = totalBytes(testMount);
        d.usable = QFileInfo(testMount).isDir();
        if (d.usable) {
            out.append(d);
        }
    }
    if (!reply.isValid()) {
        return out;
    }
    const ManagedObjects objects = reply.value();
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const InterfaceProps &ifaces = it.value();
        if (!ifaces.contains(fsIface) || !ifaces.contains(blockIface)) {
            continue;
        }
        const QVariantMap block = ifaces.value(blockIface);
        const QVariantMap fs = ifaces.value(fsIface);
        if (block.value(QStringLiteral("HintIgnore")).toBool() || block.value(QStringLiteral("IdUsage")).toString() != QLatin1String("filesystem")) {
            continue;
        }
        DriveInfo d;
        d.object = it.key().path();
        d.uuid = block.value(QStringLiteral("IdUUID")).toString();
        d.device = bytes(block.value(QStringLiteral("Device")));
        d.fsType = block.value(QStringLiteral("IdType")).toString();
        d.size = block.value(QStringLiteral("Size")).toLongLong();
        const QStringList mounts = byteArrays(fs.value(QStringLiteral("MountPoints")));
        d.mountPoint = mounts.value(0);
        if (d.uuid.isEmpty()) {
            continue;
        }

        const QString drivePath = block.value(QStringLiteral("Drive")).value<QDBusObjectPath>().path();
        const QVariantMap drive = objects.value(QDBusObjectPath(drivePath)).value(driveIface);
        const QString bus = drive.value(QStringLiteral("ConnectionBus")).toString();
        const bool removable = drive.value(QStringLiteral("Removable")).toBool() || drive.value(QStringLiteral("MediaRemovable")).toBool()
            || bus == QLatin1String("usb") || bus == QLatin1String("ieee1394") || bus == QLatin1String("sdio");
        const bool hintSystem = block.value(QStringLiteral("HintSystem")).toBool();
        const bool loop = d.device.startsWith(QLatin1String("/dev/loop"));

        bool onSystem = false;
        for (const QString &m : mounts) {
            onSystem = onSystem || systemMount(m);
        }
        d.usable = !onSystem && !block.value(QStringLiteral("ReadOnly")).toBool() && writableType(d.fsType)
            && (removable || !hintSystem);

        if (loop) {
            d.kind = QCoreApplication::translate("Drives", "Disk image");
        } else if (bus == QLatin1String("sdio") || drive.value(QStringLiteral("Media")).toString().startsWith(QLatin1String("flash_sd"))) {
            d.kind = QCoreApplication::translate("Drives", "SD card");
        } else if (bus == QLatin1String("usb")) {
            d.kind = QCoreApplication::translate("Drives", "USB drive");
        } else {
            d.kind = QCoreApplication::translate("Drives", "External drive");
        }
        const QString label = block.value(QStringLiteral("IdLabel")).toString().trimmed();
        const QString model = (drive.value(QStringLiteral("Vendor")).toString().trimmed() + QLatin1Char(' ')
                               + drive.value(QStringLiteral("Model")).toString().trimmed())
                                  .trimmed();
        d.name = !label.isEmpty() ? label : !model.isEmpty() ? model : d.kind;
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
    for (const DriveInfo &d : m_drives) {
        if (d.usable) {
            out.append(d.toMap());
        }
    }
    return out;
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
        if (!before.contains(d.uuid)) {
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
