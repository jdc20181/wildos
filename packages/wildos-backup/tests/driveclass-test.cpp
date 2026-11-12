#include "driveclass.h"

#include <QTest>

namespace
{
const QString blockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString fsIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString driveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");
const QString partIface = QStringLiteral("org.freedesktop.UDisks2.Partition");
const QString tableIface = QStringLiteral("org.freedesktop.UDisks2.PartitionTable");
const QString cryptIface = QStringLiteral("org.freedesktop.UDisks2.Encrypted");

constexpr qint64 GB = 1000000000LL;

// What UDisks2 reports for a stick, built up a block at a time.
struct Rig {
    DriveClass::Objects objects;

    QString drive(const QString &name, const QString &bus = QStringLiteral("usb"), bool removable = true)
    {
        const QString path = QStringLiteral("/org/freedesktop/UDisks2/drives/") + name;
        objects[path][driveIface] = QVariantMap{
            {QStringLiteral("Vendor"), QStringLiteral("SanDisk")},
            {QStringLiteral("Model"), QStringLiteral("Ultra")},
            {QStringLiteral("ConnectionBus"), bus},
            {QStringLiteral("Removable"), removable},
        };
        return path;
    }

    // `usage` and `type` as blkid gives them; an empty usage is a blank one.
    QString block(const QString &dev, const QString &drive, qint64 size, const QString &usage = {}, const QString &type = {},
                  const QVariantMap &extra = {}, bool partition = false)
    {
        const QString path = QStringLiteral("/org/freedesktop/UDisks2/block_devices/") + dev;
        QVariantMap b{
            {QStringLiteral("Device"), QStringLiteral("/dev/") + dev},
            {QStringLiteral("Drive"), drive.isEmpty() ? QStringLiteral("/") : drive},
            {QStringLiteral("Size"), size},
            {QStringLiteral("IdUsage"), usage},
            {QStringLiteral("IdType"), type},
            {QStringLiteral("HintSystem"), drive.isEmpty() || !objects.value(drive).value(driveIface).value(QStringLiteral("Removable")).toBool()},
            {QStringLiteral("ReadOnly"), false},
        };
        for (auto it = extra.cbegin(); it != extra.cend(); ++it) {
            b.insert(it.key(), it.value());
        }
        objects[path][blockIface] = b;
        if (usage == QLatin1String("filesystem")) {
            objects[path][fsIface] = QVariantMap{{QStringLiteral("MountPoints"), QVariantList{}}};
        }
        if (partition) {
            objects[path][partIface] = QVariantMap{{QStringLiteral("IsContainer"), false}};
        }
        return path;
    }

    void table(const QString &block)
    {
        objects[block][tableIface] = QVariantMap{{QStringLiteral("Type"), QStringLiteral("gpt")}};
    }

    void mount(const QString &block, const QString &at)
    {
        objects[block][fsIface] = QVariantMap{{QStringLiteral("MountPoints"), QVariantList{at}}};
    }

    QList<DriveInfo> shown(qint64 needed = 0) const
    {
        QList<DriveInfo> out;
        for (DriveInfo d : DriveClass::classify(objects)) {
            DriveClass::judge(d, needed);
            if (d.shown) {
                out << d;
            }
        }
        return out;
    }
};

QVariantMap label(const QString &label, const QString &uuid)
{
    return {{QStringLiteral("IdLabel"), label}, {QStringLiteral("IdUUID"), uuid}};
}
}

class DriveClassTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // The stick from the report: the installer image dd'd onto a SanDisk,
    // nothing mounted.
    void installerStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("SanDisk"));
        const QString whole = rig.block(QStringLiteral("sdb"), drive, 115 * GB, QStringLiteral("filesystem"), QStringLiteral("iso9660"),
                                        label(QStringLiteral("WILDOS"), QStringLiteral("2026-11-10-12-00-00-00")));
        rig.table(whole);
        rig.block(QStringLiteral("sdb1"), drive, 3 * GB, QStringLiteral("filesystem"), QStringLiteral("iso9660"),
                  label(QStringLiteral("WILDOS"), QStringLiteral("2026-11-10-12-00-00-00")), true);
        rig.block(QStringLiteral("sdb2"), drive, 10000000, QStringLiteral("filesystem"), QStringLiteral("vfat"),
                  label(QString(), QStringLiteral("ABCD-1234")), true);

        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("installer"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("This is a Wild OS installer stick, so it can't hold backups."));
        QCOMPARE(rows[0].device, QStringLiteral("/dev/sdb"));
        QCOMPARE(rows[0].name, QStringLiteral("WILDOS"));
        QVERIFY(!rows[0].usable);
        QVERIFY(rows[0].canFormat);
    }

    void otherDiscImage()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Stick"));
        rig.block(QStringLiteral("sdb"), drive, 16 * GB, QStringLiteral("filesystem"), QStringLiteral("udf"),
                  label(QStringLiteral("CCCOMA_X64FRE"), QStringLiteral("1234")));
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("image"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("This drive holds a disc image, so it can't hold backups."));
        QVERIFY(rows[0].canFormat);
    }

    void exfatStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Stick"));
        rig.table(rig.block(QStringLiteral("sdc"), drive, 64 * GB));
        rig.block(QStringLiteral("sdc1"), drive, 64 * GB, QStringLiteral("filesystem"), QStringLiteral("exfat"),
                  label(QStringLiteral("STICK"), QStringLiteral("1A2B-3C4D")), true);
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QVERIFY(rows[0].usable);
        QVERIFY(rows[0].reason.isEmpty());
        QVERIFY(!rows[0].canFormat);
        QCOMPARE(rows[0].name, QStringLiteral("STICK"));
        QCOMPARE(rows[0].id, QStringLiteral("1A2B-3C4D"));
    }

    void readOnlyStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Locked"));
        rig.table(rig.block(QStringLiteral("sdd"), drive, 32 * GB, {}, {}, {{QStringLiteral("ReadOnly"), true}}));
        rig.block(QStringLiteral("sdd1"), drive, 32 * GB, QStringLiteral("filesystem"), QStringLiteral("exfat"),
                  label(QStringLiteral("PHOTOS"), QStringLiteral("AAAA-BBBB")), true);
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("readOnly"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("This drive is read-only."));
        QVERIFY(!rows[0].usable);
        QVERIFY(!rows[0].canFormat);
    }

    void smallStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Small"));
        rig.block(QStringLiteral("sde"), drive, 2 * GB, QStringLiteral("filesystem"), QStringLiteral("vfat"),
                  label(QStringLiteral("OLD"), QStringLiteral("1111-2222")));
        QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("small"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("This drive is too small for your backups (2 GB, Backup needs one of 4 GB or more)."));
        QVERIFY(!rows[0].canFormat);

        rows = rig.shown(12400000000LL);
        QCOMPARE(rows[0].reasonText, QStringLiteral("This drive is too small for your backups (2 GB, your files need about 12 GB)."));
    }

    // A drive sold as 4 GB holds a little under 4 billion bytes.
    void fourGigabyteStickIsFine()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Four"));
        rig.block(QStringLiteral("sde"), drive, 3880000000LL, QStringLiteral("filesystem"), QStringLiteral("vfat"),
                  label(QString(), QStringLiteral("1111-2222")));
        QVERIFY(rig.shown()[0].usable);
    }

    void smallerThanTheFiles()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Mid"));
        rig.block(QStringLiteral("sde"), drive, 32 * GB, QStringLiteral("filesystem"), QStringLiteral("exfat"),
                  label(QString(), QStringLiteral("1111-2222")));
        const QList<DriveInfo> rows = rig.shown(120 * GB);
        QCOMPARE(rows[0].reason, QStringLiteral("small"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("This drive is too small for your backups (32 GB, your files need about 120 GB)."));
    }

    void unformattedStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Blank"));
        rig.block(QStringLiteral("sdf"), drive, 16 * GB);
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("filesystem"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("Backup can't use this drive as it is."));
        QCOMPARE(rows[0].device, QStringLiteral("/dev/sdf"));
        QCOMPARE(rows[0].name, QStringLiteral("SanDisk Ultra"));
        QVERIFY(rows[0].canFormat);
    }

    void emptyPartitionTable()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Wiped"));
        rig.table(rig.block(QStringLiteral("sdf"), drive, 16 * GB));
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("filesystem"));
        QVERIFY(rows[0].canFormat);
    }

    void unknownFilesystem()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Mac"));
        rig.table(rig.block(QStringLiteral("sdg"), drive, 500 * GB));
        rig.block(QStringLiteral("sdg1"), drive, 200000000, QStringLiteral("filesystem"), QStringLiteral("vfat"),
                  {{QStringLiteral("IdUUID"), QStringLiteral("EFI-1")}, {QStringLiteral("HintIgnore"), true}}, true);
        rig.block(QStringLiteral("sdg2"), drive, 499 * GB, QStringLiteral("filesystem"), QStringLiteral("hfsplus"),
                  label(QStringLiteral("Time Machine"), QStringLiteral("9999")), true);
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("filesystem"));
        QVERIFY(rows[0].canFormat);
    }

    // Windows' reserved partition beside NTFS gets no row of its own.
    void reservedPartitionHidden()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Passport"));
        rig.table(rig.block(QStringLiteral("sdh"), drive, 1000 * GB));
        rig.block(QStringLiteral("sdh1"), drive, 16000000, {}, {}, {}, true);
        rig.block(QStringLiteral("sdh2"), drive, 999 * GB, QStringLiteral("filesystem"), QStringLiteral("ntfs"),
                  label(QStringLiteral("My Passport"), QStringLiteral("0123ABCD")), true);
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QVERIFY(rows[0].usable);
    }

    void lockedStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Secret"));
        rig.table(rig.block(QStringLiteral("sdi"), drive, 64 * GB));
        const QString luks = rig.block(QStringLiteral("sdi1"), drive, 64 * GB, QStringLiteral("crypto"), QStringLiteral("crypto_LUKS"),
                                       {{QStringLiteral("IdUUID"), QStringLiteral("luks-1")}}, true);
        rig.objects[luks][cryptIface] = QVariantMap{{QStringLiteral("CleartextDevice"), QStringLiteral("/")}};
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].reason, QStringLiteral("locked"));
        QCOMPARE(rows[0].reasonText, QStringLiteral("Unlock this drive in Files first."));
        QVERIFY(!rows[0].canFormat);
    }

    // Once unlocked, what is inside shows instead, on the same stick.
    void unlockedStick()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Secret"));
        rig.table(rig.block(QStringLiteral("sdi"), drive, 64 * GB));
        const QString luks = rig.block(QStringLiteral("sdi1"), drive, 64 * GB, QStringLiteral("crypto"), QStringLiteral("crypto_LUKS"),
                                       {{QStringLiteral("IdUUID"), QStringLiteral("luks-1")}}, true);
        const QString clear = rig.block(QStringLiteral("dm-0"), {}, 64 * GB - 16000000, QStringLiteral("filesystem"), QStringLiteral("ext4"),
                                        {{QStringLiteral("IdUUID"), QStringLiteral("ext-1")}, {QStringLiteral("CryptoBackingDevice"), luks}});
        rig.objects[luks][cryptIface] = QVariantMap{{QStringLiteral("CleartextDevice"), clear}};
        const QList<DriveInfo> rows = rig.shown();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].uuid, QStringLiteral("ext-1"));
        QCOMPARE(rows[0].kind, QStringLiteral("USB drive"));
        QVERIFY(rows[0].usable);
    }

    void internalDiskIgnored()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("NVMe"), QString(), false);
        rig.table(rig.block(QStringLiteral("nvme0n1"), drive, 512 * GB));
        const QString root = rig.block(QStringLiteral("nvme0n1p2"), drive, 500 * GB, QStringLiteral("filesystem"), QStringLiteral("ext4"),
                                       {{QStringLiteral("IdUUID"), QStringLiteral("root")}}, true);
        rig.mount(root, QStringLiteral("/"));
        rig.block(QStringLiteral("nvme0n1p3"), drive, 10 * GB, QStringLiteral("filesystem"), QStringLiteral("ext4"),
                  {{QStringLiteral("IdUUID"), QStringLiteral("spare")}}, true);
        QVERIFY(rig.shown().isEmpty());
        // Still found by UUID, as before, for a run asking after it.
        QCOMPARE(DriveClass::classify(rig.objects).size(), 2);
    }

    // Running live from the installer, the stick is the system itself.
    void liveMediumIgnored()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("SanDisk"));
        const QString whole = rig.block(QStringLiteral("sdb"), drive, 115 * GB, QStringLiteral("filesystem"), QStringLiteral("iso9660"),
                                        label(QStringLiteral("WILDOS"), QStringLiteral("2026")));
        rig.mount(whole, QStringLiteral("/run/initramfs/live"));
        QVERIFY(rig.shown().isEmpty());
    }

    // The order the reasons are told in: an installer stick is that first,
    // even if it is small and read-only.
    void reasonOrder()
    {
        Rig rig;
        const QString drive = rig.drive(QStringLiteral("Old"));
        rig.block(QStringLiteral("sdb"), drive, 2 * GB, QStringLiteral("filesystem"), QStringLiteral("iso9660"),
                  {{QStringLiteral("IdLabel"), QStringLiteral("WILDOS")}, {QStringLiteral("ReadOnly"), true}});
        QCOMPARE(rig.shown()[0].reason, QStringLiteral("installer"));

        Rig rig2;
        const QString drive2 = rig2.drive(QStringLiteral("Old"));
        const QString block2 = rig2.block(QStringLiteral("sdb"), drive2, 2 * GB, QStringLiteral("filesystem"), QStringLiteral("hfsplus"),
                   {{QStringLiteral("IdUUID"), QStringLiteral("x")}, {QStringLiteral("ReadOnly"), true}});
        QCOMPARE(rig2.shown()[0].reason, QStringLiteral("readOnly"));
        rig2.objects[block2][blockIface][QStringLiteral("ReadOnly")] = false;
        QCOMPARE(rig2.shown()[0].reason, QStringLiteral("small"));
    }

    void acceptedFilesystems()
    {
        for (const char *t : {"ext4", "exfat", "ntfs", "ntfs3", "btrfs", "xfs", "vfat"}) {
            QVERIFY2(DriveClass::writableType(QString::fromLatin1(t)), t);
        }
        for (const char *t : {"hfsplus", "apfs", "iso9660", "squashfs", "swap", ""}) {
            QVERIFY2(!DriveClass::writableType(QString::fromLatin1(t)), t);
        }
    }
};

QTEST_GUILESS_MAIN(DriveClassTest)
#include "driveclass-test.moc"
