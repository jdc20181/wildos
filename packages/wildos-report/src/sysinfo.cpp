#include "sysinfo.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSysInfo>

#include <pwd.h>
#include <unistd.h>

using NmSettings = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(NmSettings)

namespace
{
QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QHash<QString, QString> osRelease()
{
    QString text = readFile(QStringLiteral("/usr/lib/wildos/os-release"));
    if (text.isEmpty()) {
        text = readFile(QStringLiteral("/etc/os-release"));
    }
    QHash<QString, QString> values;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq > 0) {
            QString value = line.mid(eq + 1).trimmed();
            if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }
            values.insert(line.left(eq).trimmed(), value);
        }
    }
    return values;
}

// Firmware fills unused DMI fields with placeholders.
bool meaningful(const QString &value)
{
    static const QRegularExpression filler(QStringLiteral("^(to be filled.*|default string|system product name|system manufacturer|not applicable|"
                                                          "o\\.e\\.m\\.|oem|none|unknown|0+|123456789|x+)$"),
                                           QRegularExpression::CaseInsensitiveOption);
    return !value.trimmed().isEmpty() && !filler.match(value.trimmed()).hasMatch();
}

QString run(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(500);
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString packageVersion(const QString &package)
{
    return package.isEmpty() ? QString() : run(QStringLiteral("dpkg-query"), {QStringLiteral("-W"), QStringLiteral("-f=${Version}"), package});
}

QStringList graphics()
{
    QStringList cards;
    const QDir pci(QStringLiteral("/sys/bus/pci/devices"));
    for (const QString &address : pci.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString path = pci.filePath(address);
        if (!readFile(path + QStringLiteral("/class")).startsWith(QLatin1String("0x03"))) {
            continue;
        }
        QString name;
        const QString line = run(QStringLiteral("lspci"), {QStringLiteral("-mm"), QStringLiteral("-s"), address});
        QStringList fields;
        for (auto it = QRegularExpression(QStringLiteral("\"([^\"]*)\"")).globalMatch(line); it.hasNext();) {
            fields << it.next().captured(1);
        }
        if (fields.size() >= 3) {
            name = fields.at(1) + QLatin1Char(' ') + fields.at(2);
        } else {
            name = QStringLiteral("%1:%2").arg(readFile(path + QStringLiteral("/vendor")), readFile(path + QStringLiteral("/device")));
        }
        const QFileInfo driver(path + QStringLiteral("/driver"));
        if (driver.exists()) {
            name += QStringLiteral(" (%1)").arg(QFileInfo(driver.symLinkTarget()).fileName());
        }
        cards << name;
    }
    return cards;
}

QStringList wifiNames()
{
    QStringList names;
    qDBusRegisterMetaType<NmSettings>();
    QDBusInterface settings(QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/org/freedesktop/NetworkManager/Settings"),
                            QStringLiteral("org.freedesktop.NetworkManager.Settings"), QDBusConnection::systemBus());
    if (!settings.isValid()) {
        return names;
    }
    settings.setTimeout(2000);
    const QDBusReply<QList<QDBusObjectPath>> list = settings.call(QStringLiteral("ListConnections"));
    if (!list.isValid()) {
        return names;
    }
    for (const QDBusObjectPath &path : list.value()) {
        QDBusInterface connection(QStringLiteral("org.freedesktop.NetworkManager"), path.path(),
                                  QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"), QDBusConnection::systemBus());
        connection.setTimeout(2000);
        const QDBusReply<NmSettings> reply = connection.call(QStringLiteral("GetSettings"));
        if (!reply.isValid()) {
            continue;
        }
        const NmSettings value = reply.value();
        const QVariantMap wireless = value.value(QStringLiteral("802-11-wireless"));
        if (wireless.isEmpty()) {
            continue;
        }
        const QString ssid = QString::fromUtf8(wireless.value(QStringLiteral("ssid")).toByteArray());
        const QString id = value.value(QStringLiteral("connection")).value(QStringLiteral("id")).toString();
        for (const QString &name : {ssid, id}) {
            if (!name.trimmed().isEmpty() && !names.contains(name)) {
                names << name;
            }
        }
    }
    return names;
}

// Disk serial numbers are in the names udev gives disks
// (nvme-Samsung_SSD_980_PRO_1TB_S4EVNX0R512345K), readable by anyone.
QStringList diskSerials()
{
    QStringList serials;
    static const QRegularExpression byId(QStringLiteral("^(?:ata|nvme|scsi-[0-9]|usb|mmc)-(?:.*_)?([A-Za-z0-9]{6,})(?:-0:\\d+)?(?:-part\\d+)?$"));
    static const QRegularExpression wwn(QStringLiteral("^(?:wwn|nvme-eui)[-.](0x)?([0-9A-Fa-f]{8,})(?:-part\\d+)?$"));
    for (const QString &name : QDir(QStringLiteral("/dev/disk/by-id")).entryList(QDir::System | QDir::Files | QDir::NoDotAndDotDot)) {
        auto m = wwn.match(name);
        if (m.hasMatch()) {
            serials << m.captured(2);
            continue;
        }
        m = byId.match(name);
        if (m.hasMatch()) {
            serials << m.captured(1);
        }
    }
    const QDir usb(QStringLiteral("/sys/bus/usb/devices"));
    for (const QString &device : usb.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
        const QString serial = readFile(usb.filePath(device) + QStringLiteral("/serial"));
        // Root hubs give their controller's PCI address as a serial.
        if (serial.size() >= 6 && !serial.contains(QLatin1Char(':'))) {
            serials << serial;
        }
    }
    serials.removeDuplicates();
    return serials;
}
}

namespace SysInfo
{
QString version()
{
    const auto values = osRelease();
    if (values.value(QStringLiteral("ID")) != QLatin1String("wildos")) {
        return {};
    }
    return values.value(QStringLiteral("VERSION"), values.value(QStringLiteral("VERSION_ID")));
}

QString channel()
{
    const QString sources = readFile(QStringLiteral("/etc/apt/sources.list.d/wildos.sources"));
    const auto match = QRegularExpression(QStringLiteral("^Suites:\\s*(\\S+)"), QRegularExpression::MultilineOption).match(sources);
    return match.hasMatch() ? match.captured(1) : QStringLiteral("stable");
}

QString model()
{
    const QString vendor = readFile(QStringLiteral("/sys/class/dmi/id/sys_vendor"));
    const QString product = readFile(QStringLiteral("/sys/class/dmi/id/product_name"));
    const QString family = readFile(QStringLiteral("/sys/class/dmi/id/product_version"));
    QStringList parts;
    if (meaningful(vendor)) {
        parts << vendor;
    }
    if (meaningful(product)) {
        parts << product;
    }
    // Lenovo keeps the name people know in product_version.
    if (meaningful(family) && vendor.startsWith(QLatin1String("LENOVO"), Qt::CaseInsensitive)) {
        parts << QStringLiteral("(%1)").arg(family);
    }
    return parts.join(QLatin1Char(' '));
}

QString aboutPc(const QString &package, bool screens)
{
    QStringList lines;
    const QString v = version();
    lines << QStringLiteral("Wild OS: %1, %2 updates").arg(v.isEmpty() ? QStringLiteral("unknown version") : v, channel());
    if (!package.isEmpty()) {
        const QString pv = packageVersion(package);
        if (!pv.isEmpty()) {
            lines << QStringLiteral("%1: %2").arg(package, pv);
        }
    }
    const QString kernel = QSysInfo::kernelVersion();
    lines << QStringLiteral("Kernel: %1").arg(kernel);
    const QString plasma = packageVersion(QStringLiteral("plasma-workspace"));
    if (!plasma.isEmpty()) {
        lines << QStringLiteral("Plasma: %1").arg(plasma);
    }
    const QString m = model();
    lines << QStringLiteral("Model: %1").arg(m.isEmpty() ? QStringLiteral("not given by the firmware") : m);
    const QString bios = readFile(QStringLiteral("/sys/class/dmi/id/bios_version"));
    if (meaningful(bios)) {
        lines << QStringLiteral("Firmware version: %1, %2").arg(bios, readFile(QStringLiteral("/sys/class/dmi/id/bios_date")));
    }

    const QString cpuinfo = readFile(QStringLiteral("/proc/cpuinfo"));
    const auto cpu = QRegularExpression(QStringLiteral("^model name\\s*:\\s*(.+)$"), QRegularExpression::MultilineOption).match(cpuinfo);
    const int threads = cpuinfo.count(QRegularExpression(QStringLiteral("^processor\\s*:"), QRegularExpression::MultilineOption));
    lines << QStringLiteral("Processor: %1, %2 threads").arg(cpu.hasMatch() ? cpu.captured(1).simplified() : QStringLiteral("unknown")).arg(threads);

    const QStringList cards = graphics();
    lines << QStringLiteral("Graphics: %1").arg(cards.isEmpty() ? QStringLiteral("none found") : cards.join(QStringLiteral("; ")));

    const QString meminfo = readFile(QStringLiteral("/proc/meminfo"));
    const auto total = QRegularExpression(QStringLiteral("^MemTotal:\\s*(\\d+)"), QRegularExpression::MultilineOption).match(meminfo);
    const auto available = QRegularExpression(QStringLiteral("^MemAvailable:\\s*(\\d+)"), QRegularExpression::MultilineOption).match(meminfo);
    const auto swap = QRegularExpression(QStringLiteral("^SwapTotal:\\s*(\\d+)"), QRegularExpression::MultilineOption).match(meminfo);
    const auto gb = [](const QString &kib) { return QString::number(kib.toDouble() / (1024.0 * 1024.0), 'f', 1); };
    if (total.hasMatch()) {
        lines << QStringLiteral("Memory: %1 GB, %2 GB free now, swap %3 GB")
                     .arg(gb(total.captured(1)), available.hasMatch() ? gb(available.captured(1)) : QStringLiteral("?"),
                          swap.hasMatch() ? gb(swap.captured(1)) : QStringLiteral("0"));
    }

    const bool uefi = QFileInfo::exists(QStringLiteral("/sys/firmware/efi"));
    QString firmware = uefi ? QStringLiteral("UEFI") : QStringLiteral("BIOS (legacy)");
    if (uefi) {
        QFile secure(QStringLiteral("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c"));
        if (secure.open(QIODevice::ReadOnly)) {
            const QByteArray value = secure.readAll();
            firmware += value.size() >= 5 && value.at(4) == 1 ? QStringLiteral(", Secure Boot on") : QStringLiteral(", Secure Boot off");
        }
    }
    lines << QStringLiteral("Start-up: %1").arg(firmware);

    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
    if (!session.isEmpty()) {
        lines << QStringLiteral("Session: %1").arg(session);
    }
    if (screens && qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        QStringList shown;
        for (QScreen *screen : QGuiApplication::screens()) {
            const QSize size = screen->size() * screen->devicePixelRatio();
            shown << QStringLiteral("%1 x %2 at %3%, %4 Hz")
                         .arg(size.width())
                         .arg(size.height())
                         .arg(qRound(screen->devicePixelRatio() * 100))
                         .arg(qRound(screen->refreshRate()));
        }
        if (!shown.isEmpty()) {
            lines << QStringLiteral("Screens: %1").arg(shown.join(QStringLiteral("; ")));
        }
    }
    lines << QStringLiteral("Language: %1").arg(QLocale::system().name());
    return lines.join(QLatin1Char('\n'));
}

Redactor::Known known(const QStringList &extraSerials)
{
    Redactor::Known k;
    const uid_t self = getuid();
    if (const passwd *pw = getpwuid(self)) {
        k.userName = QString::fromLocal8Bit(pw->pw_name);
        k.realName = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
    }
    setpwent();
    while (const passwd *pw = getpwent()) {
        if (pw->pw_uid < 1000 || pw->pw_uid >= 60000 || pw->pw_uid == self) {
            continue;
        }
        k.otherUsers << QString::fromLocal8Bit(pw->pw_name);
        const QString name = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
        if (!name.isEmpty()) {
            k.otherNames << name;
        }
    }
    endpwent();
    // From start-up recovery, root speaks for every account on the PC.
    if (self == 0 && k.userName == QLatin1String("root")) {
        k.userName.clear();
        k.realName.clear();
    }

    k.hostName = QSysInfo::machineHostName();
    k.wifi = wifiNames();
    k.serials = diskSerials() + extraSerials;
    k.serials.removeDuplicates();
    for (const QString &path : {QStringLiteral("/etc/machine-id"), QStringLiteral("/var/lib/dbus/machine-id"), QStringLiteral("/proc/sys/kernel/random/boot_id")}) {
        const QString id = readFile(path);
        if (id.size() >= 32) {
            k.machineIds << id << QString(id).remove(QLatin1Char('-'));
        }
    }
    k.machineIds.removeDuplicates();
    return k;
}
}
