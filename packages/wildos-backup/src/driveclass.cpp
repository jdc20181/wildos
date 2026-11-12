#include "driveclass.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QFile>
#include <QLocale>
#include <QStorageInfo>

#include <cmath>

namespace
{
const QString blockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString fsIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString driveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString partIface = QStringLiteral("org.freedesktop.UDisks2.Partition");
const QString tableIface = QStringLiteral("org.freedesktop.UDisks2.PartitionTable");
const QString cryptIface = QStringLiteral("org.freedesktop.UDisks2.Encrypted");

// The installer's ISO 9660 volume ID (build/build.sh VOLID).
const QString installerLabel = QStringLiteral("WILDOS");

// UDisks2 sends paths as NUL-terminated byte arrays.
QString bytes(const QVariant &v)
{
    if (v.metaType().id() == QMetaType::QString) {
        return v.toString();
    }
    return QString::fromUtf8(v.toByteArray().constData());
}

QStringList byteArrays(const QVariant &v)
{
    QStringList out;
    if (v.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QByteArray b;
            arg >> b;
            out << QString::fromUtf8(b.constData());
        }
        arg.endArray();
    } else {
        for (const QVariant &item : v.toList()) {
            out << bytes(item);
        }
    }
    return out;
}

QString objectPath(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<QDBusObjectPath>()) {
        return v.value<QDBusObjectPath>().path();
    }
    return v.toString();
}

bool realPath(const QString &path)
{
    return !path.isEmpty() && path != QLatin1String("/");
}

bool systemMount(const QString &mountPoint)
{
    // The last three are the live system's own medium, when Wild OS runs
    // from the stick: that stick is not one the person plugged in.
    static const QStringList system{
        QStringLiteral("/"), QStringLiteral("/home"), QStringLiteral("/boot"), QStringLiteral("/boot/efi"),
        QStringLiteral("/efi"), QStringLiteral("/usr"), QStringLiteral("/var"),
        QStringLiteral("/run/initramfs/live"), QStringLiteral("/run/live/medium"), QStringLiteral("/cdrom"),
    };
    return system.contains(mountPoint);
}

bool discType(const QString &type)
{
    return type == QLatin1String("iso9660") || type == QLatin1String("udf");
}

// make-iso.sh writes this. It can only be read when something else
// mounted the stick already; Backup never mounts one to look.
bool liveMarker(const QStringList &mounts)
{
    for (const QString &m : mounts) {
        QFile info(m + QStringLiteral("/.disk/info"));
        if (info.open(QIODevice::ReadOnly) && info.read(256).startsWith("Wild OS")) {
            return true;
        }
    }
    return false;
}

// As BackupStyle.size: decimal units, one place under 10.
QString sizeText(qint64 count)
{
    double v = double(count) / 1e9;
    QString unit = QCoreApplication::translate("Drives", "GB");
    if (v < 1) {
        v = double(count) / 1e6;
        unit = QCoreApplication::translate("Drives", "MB");
    }
    const bool decimal = v < 10 && std::abs(v - std::round(v)) >= 0.05;
    return QStringLiteral("%1 %2").arg(QLocale().toString(v, 'f', decimal ? 1 : 0), unit);
}
}

bool DriveClass::writableType(const QString &type)
{
    // Not hfsplus: Linux mounts journaled HFS+, which Macs make, read-only.
    static const QStringList ok{
        QStringLiteral("vfat"), QStringLiteral("exfat"), QStringLiteral("ntfs"), QStringLiteral("ntfs3"),
        QStringLiteral("ext2"), QStringLiteral("ext3"), QStringLiteral("ext4"), QStringLiteral("btrfs"),
        QStringLiteral("xfs"), QStringLiteral("f2fs"),
    };
    return ok.contains(type);
}

QList<DriveInfo> DriveClass::classify(const Objects &objects)
{
    // Blocks by the drive they sit on. What an unlocked encrypted drive
    // holds follows the drive it was unlocked from.
    QMap<QString, QStringList> groups;
    QStringList order;
    auto groupOf = [&objects](const QString &path) {
        QString at = path;
        for (int hops = 0; hops < 4; ++hops) {
            const QVariantMap block = objects.value(at).value(blockIface);
            const QString backing = objectPath(block.value(QStringLiteral("CryptoBackingDevice")));
            if (!realPath(backing) || !objects.contains(backing)) {
                const QString drive = objectPath(block.value(QStringLiteral("Drive")));
                return realPath(drive) ? drive : at;
            }
            at = backing;
        }
        return at;
    };
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        if (!it.value().contains(blockIface)) {
            continue;
        }
        const QString key = groupOf(it.key());
        if (!groups.contains(key)) {
            order << key;
        }
        groups[key] << it.key();
    }

    QList<DriveInfo> out;
    for (const QString &key : order) {
        const QStringList blocks = groups.value(key);
        const QVariantMap drive = objects.value(key).value(driveIface);
        const QString bus = drive.value(QStringLiteral("ConnectionBus")).toString();
        const bool removable = drive.value(QStringLiteral("Removable")).toBool() || drive.value(QStringLiteral("MediaRemovable")).toBool()
            || drive.value(QStringLiteral("Ejectable")).toBool() || bus == QLatin1String("usb") || bus == QLatin1String("ieee1394")
            || bus == QLatin1String("sdio");
        const bool optical = drive.value(QStringLiteral("Optical")).toBool();

        QString whole;
        bool onSystem = false;
        bool hotPlugged = false;
        bool installer = false;
        bool disc = false;
        QString discLabel;
        for (const QString &path : blocks) {
            const auto ifaces = objects.value(path);
            const QVariantMap block = ifaces.value(blockIface);
            if (!ifaces.contains(partIface) && !realPath(objectPath(block.value(QStringLiteral("CryptoBackingDevice"))))) {
                whole = path;
            }
        }
        for (const QString &path : blocks) {
            const auto ifaces = objects.value(path);
            const QVariantMap block = ifaces.value(blockIface);
            const QStringList mounts = byteArrays(ifaces.value(fsIface).value(QStringLiteral("MountPoints")));
            for (const QString &m : mounts) {
                onSystem = onSystem || systemMount(m);
            }
            // UDisks2 clears HintSystem for anything removable or plugged in
            // while running, such as an eSATA disk.
            hotPlugged = hotPlugged || !block.value(QStringLiteral("HintSystem"), true).toBool();
            const QString label = block.value(QStringLiteral("IdLabel")).toString().trimmed();
            if (block.value(QStringLiteral("IdUsage")).toString() == QLatin1String("filesystem")
                && discType(block.value(QStringLiteral("IdType")).toString())) {
                disc = true;
                if (discLabel.isEmpty() || path == whole) {
                    discLabel = label;
                }
                installer = installer || label.compare(installerLabel, Qt::CaseInsensitive) == 0 || liveMarker(mounts);
            }
        }
        const bool shown = (removable || hotPlugged) && !onSystem;

        QString kind;
        const QString firstDevice = bytes(objects.value(whole.isEmpty() ? blocks.value(0) : whole).value(blockIface).value(QStringLiteral("Device")));
        if (firstDevice.startsWith(QLatin1String("/dev/loop"))) {
            kind = QCoreApplication::translate("Drives", "Disk image");
        } else if (bus == QLatin1String("sdio") || drive.value(QStringLiteral("Media")).toString().startsWith(QLatin1String("flash_sd"))) {
            kind = QCoreApplication::translate("Drives", "SD card");
        } else if (bus == QLatin1String("usb")) {
            kind = QCoreApplication::translate("Drives", "USB drive");
        } else {
            kind = QCoreApplication::translate("Drives", "External drive");
        }
        const QString model = (drive.value(QStringLiteral("Vendor")).toString().trimmed() + QLatin1Char(' ')
                               + drive.value(QStringLiteral("Model")).toString().trimmed())
                                  .trimmed();
        const bool wholeReadOnly = objects.value(whole).value(blockIface).value(QStringLiteral("ReadOnly")).toBool();

        auto row = [&](const QString &path) {
            const auto ifaces = objects.value(path);
            const QVariantMap block = ifaces.value(blockIface);
            const QVariantMap fs = ifaces.value(fsIface);
            DriveInfo d;
            d.object = path;
            d.uuid = block.value(QStringLiteral("IdUUID")).toString();
            d.id = d.uuid.isEmpty() ? path : d.uuid;
            d.device = bytes(block.value(QStringLiteral("Device")));
            d.fsType = block.value(QStringLiteral("IdType")).toString();
            d.size = block.value(QStringLiteral("Size")).toLongLong();
            d.mountPoint = byteArrays(fs.value(QStringLiteral("MountPoints"))).value(0);
            d.kind = kind;
            d.shown = shown;
            d.optical = optical;
            const QString label = block.value(QStringLiteral("IdLabel")).toString().trimmed();
            d.name = !label.isEmpty() ? label : !model.isEmpty() ? model : kind;
            // A write-protect switch makes the kernel, and so UDisks2, call
            // the whole device read-only.
            bool readOnly = wholeReadOnly || block.value(QStringLiteral("ReadOnly")).toBool() || fs.value(QStringLiteral("ReadOnly")).toBool();
            if (!d.mountPoint.isEmpty()) {
                const QStorageInfo storage(d.mountPoint);
                readOnly = readOnly || (storage.isValid() && storage.isReadOnly());
            }
            if (readOnly) {
                d.problems << QStringLiteral("readOnly");
            }
            return d;
        };

        if (disc) {
            // One row for the whole stick: an installer's EFI partition and
            // the copy of the image in its first partition are all one thing.
            DriveInfo d = row(whole.isEmpty() ? blocks.value(0) : whole);
            d.id = key;
            d.problems << (installer ? QStringLiteral("installer") : QStringLiteral("image"));
            d.name = !discLabel.isEmpty() ? discLabel : !model.isEmpty() ? model : kind;
            out << d;
            continue;
        }

        QList<DriveInfo> main;
        QList<DriveInfo> spare;
        for (const QString &path : blocks) {
            const auto ifaces = objects.value(path);
            const QVariantMap block = ifaces.value(blockIface);
            const QString usage = block.value(QStringLiteral("IdUsage")).toString();
            if (block.value(QStringLiteral("HintIgnore")).toBool()) {
                continue;
            }
            if (usage == QLatin1String("filesystem")) {
                DriveInfo d = row(path);
                if (!writableType(d.fsType) || d.uuid.isEmpty()) {
                    d.problems << QStringLiteral("filesystem");
                }
                main << d;
            } else if (usage == QLatin1String("crypto")) {
                const QString clear = objectPath(ifaces.value(cryptIface).value(QStringLiteral("CleartextDevice")));
                if (realPath(clear) && objects.contains(clear)) {
                    continue;
                }
                DriveInfo d = row(path);
                d.problems << QStringLiteral("locked");
                main << d;
            } else if (ifaces.contains(tableIface) || ifaces.value(partIface).value(QStringLiteral("IsContainer")).toBool()) {
                continue;
            } else if (block.value(QStringLiteral("Size")).toLongLong() > 0) {
                // Blank, swap, or part of a RAID set.
                DriveInfo d = row(path);
                d.problems << QStringLiteral("filesystem");
                (path == whole ? main : spare) << d;
            }
        }
        // An empty partition beside a usable one is most likely a reserved
        // one Windows or a Mac made, not worth a row of its own.
        if (main.isEmpty()) {
            main = spare;
        }
        if (main.isEmpty() && !whole.isEmpty() && objects.value(whole).value(blockIface).value(QStringLiteral("Size")).toLongLong() > 0) {
            // A partition table with nothing Backup can see in it.
            DriveInfo d = row(whole);
            d.problems << QStringLiteral("filesystem");
            main << d;
        }
        out << main;
    }
    return out;
}

void DriveClass::judge(DriveInfo &d, qint64 needed)
{
    const bool small = d.size > 0 && (d.size < smallestDrive || (needed > 0 && d.size < needed));
    static const QStringList order{
        QStringLiteral("installer"), QStringLiteral("image"), QStringLiteral("readOnly"),
        QStringLiteral("small"), QStringLiteral("filesystem"), QStringLiteral("locked"),
    };
    d.reason.clear();
    for (const QString &r : order) {
        if (r == QLatin1String("small") ? small : d.problems.contains(r)) {
            d.reason = r;
            break;
        }
    }
    if (d.reason == QLatin1String("installer")) {
        d.reasonText = QCoreApplication::translate("Drives", "This is a Wild OS installer stick, so it can't hold backups.");
    } else if (d.reason == QLatin1String("image")) {
        d.reasonText = QCoreApplication::translate("Drives", "This drive holds a disc image, so it can't hold backups.");
    } else if (d.reason == QLatin1String("readOnly")) {
        d.reasonText = QCoreApplication::translate("Drives", "This drive is read-only.");
    } else if (d.reason == QLatin1String("small")) {
        d.reasonText = needed > d.size
            ? QCoreApplication::translate("Drives", "This drive is too small for your backups (%1, your files need about %2).")
                  .arg(sizeText(d.size), sizeText(needed))
            : QCoreApplication::translate("Drives", "This drive is too small for your backups (%1, Backup needs one of 4 GB or more).")
                  .arg(sizeText(d.size));
    } else if (d.reason == QLatin1String("filesystem")) {
        d.reasonText = QCoreApplication::translate("Drives", "Backup can't use this drive as it is.");
    } else if (d.reason == QLatin1String("locked")) {
        d.reasonText = QCoreApplication::translate("Drives", "Unlock this drive in Files first.");
    } else {
        d.reasonText.clear();
    }
    d.usable = d.shown && d.reason.isEmpty() && !d.uuid.isEmpty();
    d.canFormat = d.shown && !d.optical
        && (d.reason == QLatin1String("installer") || d.reason == QLatin1String("image") || d.reason == QLatin1String("filesystem"));
}
