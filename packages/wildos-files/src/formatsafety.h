#pragma once

#include <QString>
#include <QStringList>

// Which drives Files may offer to format. Kept apart from UDisks so the
// rules can be tested against a made-up /proc and /sys.
//
// `root` is prefixed to every /proc, /sys and /dev path; it is empty on a
// real system.
namespace FormatSafety
{
// The whole disks (sysfs names: "sda", "nvme0n1") a block device is built
// on, through partitions, LUKS and LVM.
QStringList baseDisks(const QString &root, const QString &blockName);

// The whole disks holding the running system: /, /boot, /home and the
// rest, swap, and the stick a live session started from.
QStringList systemDisks(const QString &root = QString());

struct Facts {
    QString blockName; // sysfs name of the volume, "sdb1"
    bool removable = false; // removable media or a hot-plugged drive
    bool optical = false;
};

bool canFormat(const QString &root, const Facts &facts);
}
