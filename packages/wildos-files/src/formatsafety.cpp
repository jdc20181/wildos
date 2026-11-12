#include "formatsafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{
QString blockDir(const QString &root, const QString &name)
{
    return root + QStringLiteral("/sys/class/block/") + name;
}

void collect(const QString &root, const QString &name, QStringList &out, int depth)
{
    if (depth > 8 || name.isEmpty()) {
        return;
    }
    const QString dir = blockDir(root, name);
    // A device mapper target (LUKS, LVM) names what it sits on in slaves/.
    const QStringList slaves = QDir(dir + QStringLiteral("/slaves")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!slaves.isEmpty()) {
        for (const QString &slave : slaves) {
            collect(root, slave, out, depth + 1);
        }
        return;
    }
    // A partition's sysfs directory sits inside its disk's.
    if (QFileInfo::exists(dir + QStringLiteral("/partition"))) {
        const QString disk = QFileInfo(QFileInfo(dir).canonicalFilePath()).dir().dirName();
        if (!disk.isEmpty() && !out.contains(disk)) {
            out.append(disk);
        }
        return;
    }
    if (!out.contains(name)) {
        out.append(name);
    }
}

// /dev/sda2, /dev/mapper/luks-... or /dev/disk/by-uuid/... to "sda2" or
// "dm-0", the names /sys uses.
QString sysName(const QString &root, const QString &device)
{
    if (!device.startsWith(QStringLiteral("/dev/"))) {
        return QString();
    }
    const QString real = QFileInfo(root + device).canonicalFilePath();
    return QFileInfo(real.isEmpty() ? device : real).fileName();
}

// Unescapes the octal escapes /proc/self/mounts uses for spaces and tabs.
QString unescape(const QByteArray &field)
{
    QByteArray out;
    for (int i = 0; i < field.size(); ++i) {
        if (field[i] == '\\' && i + 3 < field.size()) {
            bool ok = false;
            const int c = field.mid(i + 1, 3).toInt(&ok, 8);
            if (ok) {
                out.append(char(c));
                i += 3;
                continue;
            }
        }
        out.append(field[i]);
    }
    return QString::fromUtf8(out);
}

bool systemMountPoint(const QString &point)
{
    static const QStringList exact = {
        QStringLiteral("/"), QStringLiteral("/boot"), QStringLiteral("/boot/efi"), QStringLiteral("/efi"),
        QStringLiteral("/home"), QStringLiteral("/usr"), QStringLiteral("/var"), QStringLiteral("/opt"),
        QStringLiteral("/srv"), QStringLiteral("/root"), QStringLiteral("/tmp"),
    };
    // Where live-boot and dracut keep the stick a live session runs from.
    static const QStringList live = {
        QStringLiteral("/run/live/"), QStringLiteral("/lib/live/mount/"), QStringLiteral("/run/initramfs/"),
    };
    if (exact.contains(point)) {
        return true;
    }
    for (const QString &prefix : live) {
        if ((point + QLatin1Char('/')).startsWith(prefix)) {
            return true;
        }
    }
    return false;
}
}

QStringList FormatSafety::baseDisks(const QString &root, const QString &blockName)
{
    QStringList out;
    collect(root, blockName, out, 0);
    return out;
}

QStringList FormatSafety::systemDisks(const QString &root)
{
    QStringList devices;
    QFile mounts(root + QStringLiteral("/proc/self/mounts"));
    if (mounts.open(QIODevice::ReadOnly)) {
        while (!mounts.atEnd()) {
            const QList<QByteArray> fields = mounts.readLine().split(' ');
            if (fields.size() >= 2 && systemMountPoint(unescape(fields[1]))) {
                devices.append(unescape(fields[0]));
            }
        }
    }
    QFile swaps(root + QStringLiteral("/proc/swaps"));
    if (swaps.open(QIODevice::ReadOnly)) {
        swaps.readLine();
        while (!swaps.atEnd()) {
            const QByteArray line = swaps.readLine().simplified();
            if (!line.isEmpty()) {
                devices.append(unescape(line.split(' ').first()));
            }
        }
    }
    QStringList disks;
    for (const QString &device : std::as_const(devices)) {
        // A live system's / is an overlay of a squashfs on loop0, whose
        // file is on the stick; the stick's own mount is caught above.
        for (const QString &disk : baseDisks(root, sysName(root, device))) {
            if (!disks.contains(disk)) {
                disks.append(disk);
            }
        }
    }
    return disks;
}

bool FormatSafety::canFormat(const QString &root, const Facts &facts)
{
    const QString &name = facts.blockName;
    if (name.isEmpty() || facts.optical || !facts.removable) {
        return false;
    }
    static const QStringList never = {
        QStringLiteral("loop"), QStringLiteral("ram"), QStringLiteral("zram"), QStringLiteral("sr"), QStringLiteral("md"),
    };
    for (const QString &prefix : never) {
        if (name.startsWith(prefix)) {
            return false;
        }
    }
    if (!QFileInfo::exists(blockDir(root, name))) {
        return false;
    }
    const QStringList system = systemDisks(root);
    const QStringList disks = baseDisks(root, name);
    if (disks.isEmpty()) {
        return false;
    }
    for (const QString &disk : disks) {
        if (system.contains(disk)) {
            return false;
        }
    }
    return true;
}
