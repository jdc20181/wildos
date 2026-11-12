// The redactor against log lines of the kind a Wild OS report carries. All
// of it is made up: the account is Alex Morgan (alex) on alex-laptop, at
// home on "Morgan Family 5G" and at work on "BrightDesk Guest".

#include "redactor.h"

#include <QElapsedTimer>
#include <QTest>

class RedactorTest : public QObject
{
    Q_OBJECT

private:
    static Redactor::Known alex()
    {
        Redactor::Known known;
        known.userName = QStringLiteral("alex");
        known.realName = QStringLiteral("Alex Morgan");
        known.otherUsers = {QStringLiteral("sam")};
        known.otherNames = {QStringLiteral("Sam Okafor")};
        known.hostName = QStringLiteral("alex-laptop");
        known.wifi = {QStringLiteral("Morgan Family 5G"), QStringLiteral("BrightDesk Guest")};
        known.serials = {QStringLiteral("S4EVNX0R512345K"), QStringLiteral("5CG1234XYZ")};
        known.machineIds = {QStringLiteral("3f9a7c21e4b84d0f9c2e11aa5b6d7e80")};
        return known;
    }

    Redactor redactor()
    {
        Redactor r;
        r.setKnown(alex());
        return r;
    }

private Q_SLOTS:
    void userNameAndHomeFolder()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("13:09:58 files: Copying 46 items from /media/alex/KINGSTON/DCIM to /home/alex/Pictures")),
                 QStringLiteral("13:09:58 files: Copying 46 items from /media/[user]/[usb-1]/DCIM to /home/[user]/Pictures"));
        QCOMPARE(r.count(Redactor::Names), 2);
        QCOMPARE(r.count(Redactor::Files), 1);
    }

    void namedFilesAndFolders()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("Could not open /home/alex/Documents/Taxes/2025-receipts.pdf")),
                 QStringLiteral("Could not open /home/[user]/Documents/[folder-1]/[file-1]"));
        r.reset();
        QCOMPARE(r.redact(QStringLiteral("Opened \"/home/alex/Documents/Tax return/2025 receipts.pdf\" in PDF")),
                 QStringLiteral("Opened \"/home/[user]/Documents/[folder-1]/[file-1]\" in PDF"));
    }

    void hiddenProgramFoldersStay()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("kf.config: Reading /home/alex/.config/wildos/filesrc")),
                 QStringLiteral("kf.config: Reading /home/[user]/.config/wildos/filesrc"));
        QCOMPARE(r.redact(QStringLiteral("Moved '/home/alex/.local/share/Trash/files/Bank letter.odt'")),
                 QStringLiteral("Moved '/home/[user]/.local/share/Trash/files/[file-1]'"));
    }

    void tildeAndFileUrls()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("thumbnail failed for file:///home/alex/Pictures/Holiday/IMG_2041.jpg")),
                 QStringLiteral("thumbnail failed for file:///home/[user]/Pictures/[folder-1]/[file-1]"));
        QCOMPARE(r.redact(QStringLiteral("Saved ~/Notes/Wedding/speech.md.")), QStringLiteral("Saved ~/Notes/[folder-2]/[file-2]."));
    }

    void stableNumberedTokens()
    {
        Redactor r = redactor();
        const QString out = r.redact(QStringLiteral(
            "NetworkManager: device (wlp0s20f3): Activation: starting connection 'Morgan Family 5G'\n"
            "NetworkManager: device (wlp0s20f3): roamed away from 'BrightDesk Guest'\n"
            "NetworkManager: policy: set 'Morgan Family 5G' (wlp0s20f3) as default"));
        QCOMPARE(out, QStringLiteral(
                          "NetworkManager: device (wlp0s20f3): Activation: starting connection '[wifi-1]'\n"
                          "NetworkManager: device (wlp0s20f3): roamed away from '[wifi-2]'\n"
                          "NetworkManager: policy: set '[wifi-1]' (wlp0s20f3) as default"));
        QCOMPARE(r.count(Redactor::Wifi), 3);
        // A later section of the same report gets the same token.
        QCOMPARE(r.redact(QStringLiteral("Wi-Fi Morgan Family 5G signal 72%")), QStringLiteral("Wi-Fi [wifi-1] signal 72%"));
    }

    void privateAndPublicAddresses()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("dhcp4 (wlp0s20f3): address 192.168.1.47, gateway 192.168.1.1, dns 9.9.9.9")),
                 QStringLiteral("dhcp4 (wlp0s20f3): address 192.168.[ip-1], gateway 192.168.[ip-2], dns [ip-3]"));
        QCOMPARE(r.redact(QStringLiteral("listening on 127.0.0.1:631 and 10.20.30.40")),
                 QStringLiteral("listening on 127.0.0.1:631 and 10.[ip-4]"));
        QCOMPARE(r.redact(QStringLiteral("netmask 255.255.255.0 for 192.168.1.47")), QStringLiteral("netmask 255.255.255.0 for 192.168.[ip-1]"));
        const QString kernel = QStringLiteral("Kernel: 6.6.87.2-microsoft-standard, 6.12.48+deb13-amd64");
        QCOMPARE(r.redact(kernel), kernel);
    }

    void ipv6Addresses()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("address fe80::1c2b:3dff:fe4e:5f60/64 and 2a02:c7c:1234:5600::1a on wlp0s20f3, loopback ::1")),
                 QStringLiteral("address [ip-1]/64 and [ip-2] on wlp0s20f3, loopback ::1"));
    }

    void timesAndCodeAreNotAddresses()
    {
        Redactor r = redactor();
        const QString line = QStringLiteral("13:10:06 kwin_wayland: std::vector<KWin::Window*> at 0x7f3a2c, version 6.3.4, at 13:10:06.521");
        QCOMPARE(r.redact(line), line);
        QCOMPARE(r.total(), 0);
    }

    void macAddresses()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("wlp0s20f3: associated with 3C:84:6A:12:9B:F0, own 8c:17:59:aa:01:2e, broadcast ff:ff:ff:ff:ff:ff")),
                 QStringLiteral("wlp0s20f3: associated with [mac-1], own [mac-2], broadcast ff:ff:ff:ff:ff:ff"));
        // The same address written another way is the same device.
        QCOMPARE(r.redact(QStringLiteral("bluetoothd: 3c-84-6a-12-9b-f0 connected")), QStringLiteral("bluetoothd: [mac-1] connected"));
    }

    void serialNumbers()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("usb 3-2: SerialNumber: 0401a1b2c3d4e5f60718")), QStringLiteral("usb 3-2: SerialNumber: [serial-1]"));
        QCOMPARE(r.redact(QStringLiteral("nvme0: Samsung SSD 980 PRO 1TB, S4EVNX0R512345K")), QStringLiteral("nvme0: Samsung SSD 980 PRO 1TB, [serial-2]"));
        QCOMPARE(r.redact(QStringLiteral("Serial Number: 5CG1234XYZ")), QStringLiteral("Serial Number: [serial-3]"));
    }

    void machineIds()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("Machine ID: 3f9a7c21e4b84d0f9c2e11aa5b6d7e80\nBoot ID: 0c8d6b1e2f3a4b5c9d8e7f6a5b4c3d2e")),
                 QStringLiteral("Machine ID: [id-1]\nBoot ID: [id-2]"));
        QCOMPARE(r.redact(QStringLiteral("Found device /dev/disk/by-uuid/7b1f0c2e-9a44-4c1d-8e2f-5a6b7c8d9e0f")),
                 QStringLiteral("Found device /dev/disk/by-uuid/[id-3]"));
        QCOMPARE(r.count(Redactor::Serials), 3);
    }

    void emails()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("mail: sync failed for alex.morgan@example.org (imap.example.org)")),
                 QStringLiteral("mail: sync failed for [email-1] (imap.example.org)"));
        QCOMPARE(r.count(Redactor::Emails), 1);
        QCOMPARE(r.count(Redactor::Names), 0);
        const QString units = QStringLiteral("systemd: Started systemd-coredump@0-22389-0.service and user@1001.service.");
        QCOMPARE(r.redact(units), units);
    }

    void namesHostAndOtherAccounts()
    {
        Redactor r = redactor();
        QCOMPARE(r.redact(QStringLiteral("sudo: alex : TTY=pts/0 ; PWD=/home/alex ; USER=root ; COMMAND=/usr/bin/apt update")),
                 QStringLiteral("sudo: [user] : TTY=pts/0 ; PWD=/home/[user] ; USER=root ; COMMAND=/usr/bin/apt update"));
        QCOMPARE(r.redact(QStringLiteral("alex-laptop systemd-logind: New session 3 of user alex (Alex Morgan).")),
                 QStringLiteral("[pc] systemd-logind: New session 3 of user [user] ([name])."));
        QCOMPARE(r.redact(QStringLiteral("Morgan signed in; sam's files are in /home/sam, owned by Sam Okafor")),
                 QStringLiteral("[name] signed in; [account-1]'s files are in /home/[account-1], owned by [account-2]"));
        QCOMPARE(r.count(Redactor::PcName), 1);
    }

    void wordsInsideOtherWordsStay()
    {
        Redactor r;
        Redactor::Known known = alex();
        known.userName = QStringLiteral("sam");
        r.setKnown(known);
        const QString line = QStringLiteral("Sample rate 48000, sampler ready");
        QCOMPARE(r.redact(line), line);
    }

    void genericHostNameIsKept()
    {
        Redactor r;
        Redactor::Known known = alex();
        known.hostName = QStringLiteral("wildos");
        r.setKnown(known);
        const QString line = QStringLiteral("wildos-files: started");
        QCOMPARE(r.redact(line), line);
    }

    void customWords()
    {
        Redactor r = redactor();
        r.setCustomWords({QStringLiteral("Project Falcon"), QStringLiteral("  "), QStringLiteral("acme")});
        QCOMPARE(r.redact(QStringLiteral("Write: saved Project Falcon budget for ACME and Acme Ltd")),
                 QStringLiteral("Write: saved [word-1] budget for [word-2] and [word-2] Ltd"));
        QCOMPARE(r.count(Redactor::Custom), 3);
    }

    void resetStartsAgain()
    {
        Redactor r = redactor();
        r.redact(QStringLiteral("from 192.168.1.47 on Morgan Family 5G"));
        QCOMPARE(r.total(), 2);
        r.reset();
        QCOMPARE(r.total(), 0);
        QCOMPARE(r.redact(QStringLiteral("BrightDesk Guest")), QStringLiteral("[wifi-1]"));
    }

    void bigLogStaysFast()
    {
        Redactor r = redactor();
        QString log;
        for (int i = 0; i < 20000; ++i) {
            log += QStringLiteral("13:10:%1 wildos-files[4127]: thumbnail for /home/alex/Pictures/Holiday/IMG_%2.jpg from 192.168.1.%3\n")
                       .arg(i % 60, 2, 10, QLatin1Char('0'))
                       .arg(i)
                       .arg(i % 250);
        }
        QElapsedTimer timer;
        timer.start();
        const QString out = r.redact(log);
        QVERIFY(!out.contains(QLatin1String("alex")));
        QVERIFY(!out.contains(QLatin1String("Holiday")));
        QVERIFY(timer.elapsed() < 10000);
    }
};

QTEST_GUILESS_MAIN(RedactorTest)
#include "redactor_test.moc"
