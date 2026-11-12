#include "formatsafety.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// A made-up machine: Wild OS on sda (EFI on sda1, LUKS root on sda3 as
// dm-0), a live stick on sdc, swap on nvme0n1, and a USB stick on sdb
// with an ordinary partition and an unlocked LUKS one (dm-1).
class FormatSafetyTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString root() const { return m_dir.path(); }

    void touch(const QString &path)
    {
        QDir().mkpath(QFileInfo(root() + path).path());
        QFile f(root() + path);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }

    void disk(const QString &name, const QStringList &partitions)
    {
        QDir().mkpath(root() + QStringLiteral("/sys/block/") + name);
        QFile::link(QStringLiteral("../../block/") + name, root() + QStringLiteral("/sys/class/block/") + name);
        touch(QStringLiteral("/dev/") + name);
        for (const QString &part : partitions) {
            touch(QStringLiteral("/sys/block/%1/%2/partition").arg(name, part));
            QFile::link(QStringLiteral("../../block/%1/%2").arg(name, part), root() + QStringLiteral("/sys/class/block/") + part);
            touch(QStringLiteral("/dev/") + part);
        }
    }

    void mapper(const QString &dm, const QString &slave, const QString &mapperName)
    {
        QDir().mkpath(root() + QStringLiteral("/sys/block/%1/slaves/%2").arg(dm, slave));
        QFile::link(QStringLiteral("../../block/") + dm, root() + QStringLiteral("/sys/class/block/") + dm);
        touch(QStringLiteral("/dev/") + dm);
        QDir().mkpath(root() + QStringLiteral("/dev/mapper"));
        QFile::link(QStringLiteral("../") + dm, root() + QStringLiteral("/dev/mapper/") + mapperName);
    }

    bool can(const QString &name, bool removable = true, bool optical = false)
    {
        FormatSafety::Facts facts;
        facts.blockName = name;
        facts.removable = removable;
        facts.optical = optical;
        return FormatSafety::canFormat(root(), facts);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QDir().mkpath(root() + QStringLiteral("/sys/class/block"));
        disk(QStringLiteral("sda"), {QStringLiteral("sda1"), QStringLiteral("sda2"), QStringLiteral("sda3")});
        disk(QStringLiteral("sdb"), {QStringLiteral("sdb1"), QStringLiteral("sdb2")});
        disk(QStringLiteral("sdc"), {QStringLiteral("sdc1"), QStringLiteral("sdc2")});
        disk(QStringLiteral("nvme0n1"), {QStringLiteral("nvme0n1p1")});
        disk(QStringLiteral("loop0"), {});
        disk(QStringLiteral("sr0"), {});
        mapper(QStringLiteral("dm-0"), QStringLiteral("sda3"), QStringLiteral("luks-0a1b"));
        mapper(QStringLiteral("dm-1"), QStringLiteral("sdb2"), QStringLiteral("luks-9f8e"));

        QDir().mkpath(root() + QStringLiteral("/proc/self"));
        QFile mounts(root() + QStringLiteral("/proc/self/mounts"));
        QVERIFY(mounts.open(QIODevice::WriteOnly));
        mounts.write("/dev/mapper/luks-0a1b / ext4 rw,relatime 0 0\n"
                     "proc /proc proc rw 0 0\n"
                     "/dev/sda1 /boot/efi vfat rw 0 0\n"
                     "/dev/sdc1 /run/live/medium iso9660 ro 0 0\n"
                     "/dev/loop0 /run/live/rootfs/filesystem.squashfs squashfs ro 0 0\n"
                     "/dev/sdb1 /media/alex/HOLIDAY\\040PHOTOS exfat rw 0 0\n"
                     "/dev/mapper/luks-9f8e /media/alex/Private ext4 rw 0 0\n");
        mounts.close();
        QFile swaps(root() + QStringLiteral("/proc/swaps"));
        QVERIFY(swaps.open(QIODevice::WriteOnly));
        swaps.write("Filename\tType\tSize\tUsed\tPriority\n/dev/nvme0n1p1 partition 8388604 0 -2\n");
    }

    void baseDisksFollowsPartitionsAndLuks()
    {
        QCOMPARE(FormatSafety::baseDisks(root(), QStringLiteral("sda2")), QStringList{QStringLiteral("sda")});
        QCOMPARE(FormatSafety::baseDisks(root(), QStringLiteral("dm-0")), QStringList{QStringLiteral("sda")});
        QCOMPARE(FormatSafety::baseDisks(root(), QStringLiteral("sdb")), QStringList{QStringLiteral("sdb")});
    }

    void systemDisksAreRootBootLiveAndSwap()
    {
        QStringList disks = FormatSafety::systemDisks(root());
        disks.sort();
        QCOMPARE(disks, (QStringList{QStringLiteral("loop0"), QStringLiteral("nvme0n1"), QStringLiteral("sda"), QStringLiteral("sdc")}));
    }

    void usbStickCanBeFormatted()
    {
        QVERIFY(can(QStringLiteral("sdb1")));
        QVERIFY(can(QStringLiteral("sdb")));
        QVERIFY(can(QStringLiteral("sdb2")));
        // Its unlocked encrypted partition, which formats as its container.
        QVERIFY(can(QStringLiteral("dm-1")));
    }

    void systemDiskNever()
    {
        // Even if the system disk claims to be removable, as some USB
        // enclosures and SD card readers do.
        QVERIFY(!can(QStringLiteral("sda")));
        QVERIFY(!can(QStringLiteral("sda1")));
        QVERIFY(!can(QStringLiteral("sda2")));
        QVERIFY(!can(QStringLiteral("sda3")));
        QVERIFY(!can(QStringLiteral("dm-0")));
    }

    void liveStickNever()
    {
        QVERIFY(!can(QStringLiteral("sdc")));
        QVERIFY(!can(QStringLiteral("sdc1")));
        // Its other partition is on the same stick.
        QVERIFY(!can(QStringLiteral("sdc2")));
    }

    void swapDiskNever()
    {
        QVERIFY(!can(QStringLiteral("nvme0n1")));
        QVERIFY(!can(QStringLiteral("nvme0n1p1")));
    }

    void internalDrivesAndOthersNever()
    {
        QVERIFY(!can(QStringLiteral("sdb1"), false));
        QVERIFY(!can(QStringLiteral("sr0")));
        QVERIFY(!can(QStringLiteral("sdb1"), true, true));
        QVERIFY(!can(QStringLiteral("loop0")));
        QVERIFY(!can(QStringLiteral("sdz1")));
        QVERIFY(!can(QString()));
    }
};

QTEST_GUILESS_MAIN(FormatSafetyTest)
#include "formatsafety_test.moc"
