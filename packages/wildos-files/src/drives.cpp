#include "drives.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <Solid/Block>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDisc>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

namespace
{
bool userMountPath(const QString &path)
{
    // Places a desktop mounts things for people. Everything else that is
    // mounted (/boot, /home on its own partition, /var) is part of the
    // system and stays out of sight, like the hidden partitions on Windows.
    static const QStringList roots = {QStringLiteral("/media/"), QStringLiteral("/run/media/"), QStringLiteral("/mnt/")};
    for (const QString &root : roots) {
        if (path.startsWith(root)) {
            return true;
        }
    }
    return false;
}

// Whether a block device sits on a LUKS container, directly or through
// LVM: device mapper names its crypt targets CRYPT-LUKS..., and every
// layer lists what it is built on under slaves/.
bool onLuks(const QString &sysName, int depth = 0)
{
    if (depth > 4 || sysName.isEmpty()) {
        return false;
    }
    QFile uuid(QStringLiteral("/sys/class/block/%1/dm/uuid").arg(sysName));
    if (uuid.open(QIODevice::ReadOnly) && uuid.readAll().startsWith("CRYPT-LUKS")) {
        return true;
    }
    const QStringList slaves = QDir(QStringLiteral("/sys/class/block/%1/slaves").arg(sysName)).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &slave : slaves) {
        if (onLuks(slave, depth + 1)) {
            return true;
        }
    }
    return false;
}

bool rootEncrypted()
{
    // /dev/mapper/luks-... is a link to /dev/dm-N, whose name /sys uses.
    const QString device = QString::fromLocal8Bit(QStorageInfo(QStringLiteral("/")).device());
    return onLuks(QFileInfo(QFileInfo(device).canonicalFilePath()).fileName());
}

bool isNtfs(const QString &fsType)
{
    return fsType == QLatin1String("ntfs") || fsType == QLatin1String("ntfs3") || fsType == QLatin1String("ntfs-3g");
}

QString sizeName(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("Drive");
    }
    // Drives are sold in decimal sizes, so a 500 GB stick reads 500 GB.
    double value = bytes / 1e9;
    QString unit = QStringLiteral("GB");
    if (value >= 1000) {
        value /= 1000;
        unit = QStringLiteral("TB");
    }
    QString number = QString::number(value, 'f', value < 10 ? 1 : 0);
    if (number.endsWith(QStringLiteral(".0"))) {
        number.chop(2);
    }
    return number + QLatin1Char(' ') + unit + QStringLiteral(" drive");
}
}

Drives *Drives::instance()
{
    static Drives *drives = new Drives;
    return drives;
}

Drives::Drives(QObject *parent)
    : QObject(parent)
{
    connect(this, &Drives::changed, this, [this] {
        m_dirty = true;
    });
    auto notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &udi) {
        watch(udi);
        Q_EMIT changed();
    });
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &Drives::changed);
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        watch(device.udi());
    }
}

void Drives::watch(const QString &udi)
{
    Solid::Device device(udi);
    auto access = device.as<Solid::StorageAccess>();
    if (!access) {
        return;
    }
    connect(access, &Solid::StorageAccess::accessibilityChanged, this, &Drives::changed, Qt::UniqueConnection);
}

QList<Drive> Drives::list() const
{
    // Asking udisks is a D-Bus round trip per device, and titles and
    // breadcrumbs ask often; the answer only changes when a device does.
    if (m_dirty) {
        m_cache = scan();
        m_dirty = false;
    }
    return m_cache;
}

QList<Drive> Drives::scan() const
{
    QList<Drive> drives;

    Drive system;
    system.name = QStringLiteral("Wild OS");
    system.chip = rootEncrypted() || qEnvironmentVariableIsSet("WILDOS_FILES_RIG_ENCRYPTED") ? QStringLiteral("Encrypted") : QString();
    system.themeIcon = QStringLiteral("drive-harddisk");
    system.lineIcon = QStringLiteral("drive");
    system.mountPath = QStringLiteral("/");
    system.mounted = true;
    system.system = true;
    const QStorageInfo root(QStringLiteral("/"));
    system.total = root.bytesTotal();
    system.free = root.bytesAvailable();
    drives.append(system);

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        auto access = device.as<Solid::StorageAccess>();
        auto volume = device.as<Solid::StorageVolume>();
        auto block = device.as<Solid::Block>();
        // Solid's StorageAccess::isIgnored() also says yes to every volume
        // that isn't mounted (its path is empty, so not under /media), which
        // hid a USB stick allowed through USBGuard until something mounted
        // it. Hidden volumes are caught by StorageVolume::isIgnored() below,
        // and mounts outside the user's paths by userMountPath().
        if (!access || (access->isAccessible() && access->isIgnored())) {
            continue;
        }
        if (volume && (volume->isIgnored() || volume->usage() != Solid::StorageVolume::FileSystem)) {
            continue;
        }
        if (block && block->device().startsWith(QStringLiteral("/dev/loop"))) {
            continue;
        }
        if (volume && (volume->fsType() == QStringLiteral("swap") || volume->fsType() == QStringLiteral("squashfs"))) {
            continue;
        }
        const bool ntfs = volume && isNtfs(volume->fsType());
        const QString path = access->filePath();
        if (access->isAccessible() && !userMountPath(path + QLatin1Char('/'))) {
            continue;
        }

        Drive drive;
        drive.udi = device.udi();
        drive.mounted = access->isAccessible();
        drive.mountPath = drive.mounted ? path : QString();

        bool removable = false;
        const bool optical = device.is<Solid::OpticalDisc>();
        for (Solid::Device parent = device.parent(); parent.isValid(); parent = parent.parent()) {
            if (auto storage = parent.as<Solid::StorageDrive>()) {
                removable = storage->isRemovable() || storage->isHotpluggable();
                break;
            }
        }
        drive.removable = removable || optical;
        // Windows keeps its recovery tools on a small NTFS partition it
        // never shows; neither does Files.
        if (ntfs && !drive.removable && volume && volume->size() < 2000000000ULL) {
            continue;
        }
        const bool windowsSystem = ntfs && drive.mounted && QFileInfo::exists(drive.mountPath + QStringLiteral("/Windows/System32"));
        drive.chip = drive.removable ? QStringLiteral("Removable") : ntfs ? QStringLiteral("Windows") : QString();
        const QString label = volume ? volume->label().trimmed() : QString();
        drive.total = volume ? qint64(volume->size()) : 0;
        if (drive.mounted) {
            const QStorageInfo info(drive.mountPath);
            drive.free = info.bytesAvailable();
            if (info.bytesTotal() > 0) {
                drive.total = info.bytesTotal();
            }
        }
        if (windowsSystem) {
            drive.name = QStringLiteral("Windows (C:)");
        } else if (!label.isEmpty()) {
            drive.name = label;
        } else if (optical) {
            drive.name = QStringLiteral("Disc");
        } else if (drive.removable) {
            drive.name = QStringLiteral("USB drive");
        } else {
            drive.name = sizeName(volume ? qint64(volume->size()) : drive.total);
        }
        drive.themeIcon = optical ? QStringLiteral("media-optical")
            : drive.removable ? QStringLiteral("drive-removable-media-usb")
            : QStringLiteral("drive-harddisk");
        drive.lineIcon = drive.removable ? QStringLiteral("usb") : QStringLiteral("drive");
        drives.append(drive);
    }

    // The design rig has no udisks to report a Windows partition or a USB
    // stick: WILDOS_FILES_RIG_DRIVES="name|chip|removable|total|free|path;..."
    // adds drives as the code above would have named them.
    const QStringList fakes = qEnvironmentVariable("WILDOS_FILES_RIG_DRIVES").split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &fake : fakes) {
        const QStringList f = fake.split(QLatin1Char('|'));
        if (f.size() < 6) {
            continue;
        }
        Drive drive;
        drive.name = f[0];
        drive.chip = f[1];
        drive.removable = f[2] == QLatin1String("1");
        drive.total = f[3].toLongLong();
        drive.free = f[4].toLongLong();
        drive.mountPath = f[5];
        drive.mounted = true;
        drive.udi = QStringLiteral("/rig/") + drive.name;
        drive.themeIcon = drive.removable ? QStringLiteral("drive-removable-media-usb") : QStringLiteral("drive-harddisk");
        drive.lineIcon = drive.removable ? QStringLiteral("usb") : QStringLiteral("drive");
        drives.append(drive);
    }
    return drives;
}

Drive Drives::driveFor(const QString &path) const
{
    Drive best;
    qsizetype bestLength = -1;
    const auto all = list();
    for (const Drive &drive : all) {
        if (!drive.mounted) {
            continue;
        }
        const QString root = drive.mountPath.endsWith(QLatin1Char('/')) ? drive.mountPath : drive.mountPath + QLatin1Char('/');
        if ((path + QLatin1Char('/')).startsWith(root) && root.size() > bestLength) {
            best = drive;
            bestLength = root.size();
        }
    }
    return best;
}

void Drives::mount(const QString &udi)
{
    Solid::Device device(udi);
    auto access = device.as<Solid::StorageAccess>();
    if (!access) {
        return;
    }
    if (access->isAccessible()) {
        Q_EMIT mounted(udi, access->filePath());
        return;
    }
    connect(access, &Solid::StorageAccess::setupDone, this, [this, udi](Solid::ErrorType error, const QVariant &, const QString &) {
        Solid::Device device(udi);
        auto access = device.as<Solid::StorageAccess>();
        if (error == Solid::NoError && access) {
            Q_EMIT mounted(udi, access->filePath());
        } else if (error != Solid::UserCanceled) {
            Q_EMIT failed(QStringLiteral("This drive could not be opened. Check that it is plugged in properly and try again."));
        }
    }, Qt::SingleShotConnection);
    access->setup();
}

void Drives::eject(const QString &udi)
{
    Solid::Device device(udi);
    if (auto optical = device.as<Solid::OpticalDrive>()) {
        optical->eject();
        return;
    }
    auto access = device.as<Solid::StorageAccess>();
    if (!access) {
        return;
    }
    connect(access, &Solid::StorageAccess::teardownDone, this, [this](Solid::ErrorType error, const QVariant &, const QString &) {
        if (error != Solid::NoError && error != Solid::UserCanceled) {
            Q_EMIT failed(QStringLiteral("This drive is still in use. Close any files or apps using it, then try again."));
        }
        Q_EMIT changed();
    }, Qt::SingleShotConnection);
    access->teardown();
}
