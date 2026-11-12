#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// One drive or filesystem UDisks2 knows about, and whether Backup may use
// it. Internal disks are kept too (shown false) so the scheduled run can
// still find a drive the person picked by its UUID.
struct DriveInfo {
    // Unique in a scan: the filesystem UUID, or the UDisks2 object path for
    // something without one (a blank stick, a locked drive).
    QString id;
    QString uuid;
    QString object; // the block device's UDisks2 object path
    QString device;
    QString name;
    QString kind;
    QString fsType;
    qint64 size = 0;
    QString mountPoint;
    // Plugged in by the person: removable or hot-plugged, and holding
    // nothing of the running system.
    bool shown = false;
    // Every reason Backup can't use it, unordered; DriveClass::judge picks
    // the one to tell.
    QStringList problems;
    // Set by judge: the reason shown, or empty when Backup may use it.
    QString reason;
    QString reasonText;
    bool usable = false;
    bool canFormat = false;
    bool optical = false;

    QVariantMap toMap() const;
};

// Sorting what UDisks2 reports into drives Backup can use and drives it
// can't, from properties alone: a drive is never mounted to find out.
namespace DriveClass
{
// GetManagedObjects as plain values: object path, then interface, then
// properties. Object paths and byte strings may be plain strings, so tests
// and the test rig can write them by hand.
using Objects = QMap<QString, QMap<QString, QVariantMap>>;

// Under this a drive holds too little to be worth backing up to. A stick
// sold as 4 GB holds a little under 4 billion bytes, so it still passes.
constexpr qint64 smallestDrive = 3500000000LL;

// Filesystems Backup writes to. restic's packs are 16 MiB, well inside
// FAT32's 4 GB file limit, so FAT32 is fine.
bool writableType(const QString &type);

QList<DriveInfo> classify(const Objects &objects);

// Settles reason, usable and canFormat, checking size against `needed`,
// the bytes the backup would hold (0 or less when not known yet).
void judge(DriveInfo &drive, qint64 needed);
}
