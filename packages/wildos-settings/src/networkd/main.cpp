// wildos-network: the part of Advanced network settings that runs as root.
//
// Encrypted DNS by default. NetworkManager has no setting for a default DNS
// server, and systemd-resolved's global servers would be asked alongside a
// network's own rather than ahead of them. So each Wi-Fi and Ethernet
// profile is given Quad9 the first time this service sees it: asked first,
// over TLS wherever the network allows, with the network's own servers kept
// behind it so sign-in pages on hotel and café Wi-Fi still load (see
// NetShared::dnsModifyArgs). A profile that already names its own DNS
// servers is left as it is, and so is anything the person changes
// afterwards: each profile is only ever defaulted once.
//
// The VPN kill switch, when /etc/wildos/vpn-kill-switch exists. From the
// moment a VPN is connected, nothing leaves this computer except through a
// VPN, to the VPN's own servers (so it can reconnect), on the local network
// (printers, screens) or to keep the network itself up (DHCP, IPv6
// neighbour discovery). If the VPN drops, that stays so until it connects
// again or is switched off: NetworkManager says when someone turned it off
// (reason "user-disconnected"), and Settings says so too (Release). A
// WireGuard tunnel has no connection to lose, so it only ever goes down
// because it was turned off. Lookups leave through systemd-resolved only
// when encrypted (DNS over TLS); plain DNS outside the tunnel is refused.
// While the Mullvad app's own tunnel is up, its kill switch is in charge
// and this one stands aside rather than fight it.

#include "netshared.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHash>
#include <QHostAddress>
#include <QHostInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

#include <iterator>
#include <utility>

#include <pwd.h>

namespace
{
const QString configDir = QStringLiteral("/etc/wildos");
const QString killSwitchFile = QStringLiteral("/etc/wildos/vpn-kill-switch");
const QString runDir = QStringLiteral("/run/wildos");
const QString stateFile = QStringLiteral("/run/wildos/kill-switch");
const QString armedFile = QStringLiteral("/run/wildos/kill-switch.armed");
const QString defaultedFile = QStringLiteral("/var/lib/wildos/network-dns-defaulted");
const QString table = QStringLiteral("wildos_kill_switch");

const QString wifiType = QStringLiteral("802-11-wireless");
const QString wiredType = QStringLiteral("802-3-ethernet");

// NMActiveConnectionState and NMActiveConnectionStateReason
constexpr uint deactivated = 4;
constexpr uint userDisconnected = 2;
constexpr uint connectionRemoved = 11;

// NM_SETTINGS_CONNECTION_FLAG_EXTERNAL: a profile NetworkManager made up for
// an interface something else runs.
constexpr uint externalFlag = 0x8;

const QStringList localV4{QStringLiteral("10.0.0.0/8"), QStringLiteral("172.16.0.0/12"), QStringLiteral("192.168.0.0/16"),
                         QStringLiteral("169.254.0.0/16"), QStringLiteral("224.0.0.0/4"), QStringLiteral("255.255.255.255")};
const QStringList localV6{QStringLiteral("fe80::/10"), QStringLiteral("fc00::/7"), QStringLiteral("ff00::/8")};

struct Active {
    QString uuid;
    QString type;
    QString device;
    QString state;
    QString path;
    bool saved = false;
};

struct Armed {
    QString type;
    QStringList addresses;
};

QString run(const QString &program, const QStringList &args, bool *ok = nullptr, const QByteArray &input = QByteArray())
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(env);
    process.start(program, args);
    if (!input.isEmpty()) {
        process.write(input);
    }
    process.closeWriteChannel();
    const bool finished = process.waitForFinished(30000);
    const bool success = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!success) {
        qWarning().noquote() << program << args.join(QLatin1Char(' ')) << QString::fromUtf8(process.readAllStandardError()).trimmed();
    }
    if (ok) {
        *ok = success;
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

QString nmcli(const QStringList &args, bool *ok = nullptr)
{
    return run(QStringLiteral("nmcli"), args, ok);
}

uint connectionFlags(const QString &path)
{
    QDBusMessage call = QDBusMessage::createMethodCall(NetShared::nmService, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                       QStringLiteral("Get"));
    call << QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection") << QStringLiteral("Flags");
    const QDBusMessage reply = QDBusConnection::systemBus().call(call, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return 0;
    }
    return reply.arguments().constFirst().value<QDBusVariant>().variant().toUInt();
}

QStringList readLines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void writeFile(const QString &path, const QString &text)
{
    QDir().mkpath(path.section(QLatin1Char('/'), 0, -2));
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(text.toUtf8());
        file.commit();
    }
}

bool isVpn(const QString &type)
{
    return type == QLatin1String("vpn") || type == QLatin1String("wireguard");
}
}

class NetworkService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Network")

public:
    NetworkService()
    {
        m_timer.setSingleShot(true);
        m_timer.setInterval(300);
        connect(&m_timer, &QTimer::timeout, this, &NetworkService::reconcile);

        for (const QString &uuid : readLines(defaultedFile)) {
            m_defaulted.insert(uuid.trimmed());
        }
        for (const QString &line : readLines(armedFile)) {
            const QStringList parts = line.split(QLatin1Char(' '));
            if (parts.size() >= 2) {
                m_armed.insert(parts.at(0), Armed{parts.at(1), parts.value(2).split(QLatin1Char(','), Qt::SkipEmptyParts)});
            }
        }

        QDBusConnection bus = QDBusConnection::systemBus();
        bus.connect(NetShared::nmService, QString(), QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"), QStringLiteral("StateChanged"),
                    this, SLOT(activeStateChanged(uint, uint, QDBusMessage)));
        bus.connect(NetShared::nmService, QStringLiteral("/org/freedesktop/NetworkManager/Settings"), QStringLiteral("org.freedesktop.NetworkManager.Settings"),
                    QStringLiteral("NewConnection"), this, SLOT(schedule()));
        bus.connect(NetShared::nmService, QStringLiteral("/org/freedesktop/NetworkManager/Settings"), QStringLiteral("org.freedesktop.NetworkManager.Settings"),
                    QStringLiteral("ConnectionRemoved"), this, SLOT(schedule()));
        auto watcher = new QDBusServiceWatcher(NetShared::nmService, bus, QDBusServiceWatcher::WatchForRegistration, this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &NetworkService::schedule);

        QDir().mkpath(configDir);
        m_files.addPath(configDir);
        connect(&m_files, &QFileSystemWatcher::directoryChanged, this, &NetworkService::schedule);
        // Mullvad's tunnel comes and goes without NetworkManager saying so.
        m_poll.setInterval(10000);
        connect(&m_poll, &QTimer::timeout, this, &NetworkService::schedule);
        m_poll.start();

        bus.registerObject(QStringLiteral("/org/wildos/Network"), this, QDBusConnection::ExportScriptableSlots);
        bus.registerService(QStringLiteral("org.wildos.Network"));

        reconcile();
    }

public Q_SLOTS:
    // The person switched this VPN off in Settings: if it is already down,
    // stop holding the internet back for it; if it is still up, its
    // disconnect is a deliberate one.
    Q_SCRIPTABLE void Release(const QString &uuid)
    {
        bool up = false;
        for (const Active &a : m_active) {
            up = up || (a.uuid == uuid && a.state == QLatin1String("activated"));
        }
        if (up) {
            m_userOff.insert(uuid);
        } else {
            m_armed.remove(uuid);
        }
        schedule();
    }

private Q_SLOTS:
    void schedule()
    {
        m_timer.start();
    }

    void activeStateChanged(uint state, uint reason, const QDBusMessage &message)
    {
        if (state == deactivated && (reason == userDisconnected || reason == connectionRemoved)) {
            const QString uuid = m_pathToUuid.value(message.path());
            if (!uuid.isEmpty()) {
                m_userOff.insert(uuid);
            }
        }
        schedule();
    }

    void reconcile()
    {
        readActive();
        const auto all = readAll();
        stampDefaults(all);
        killSwitch(all);
    }

private:
    void readActive()
    {
        m_active.clear();
        bool ok = false;
        const QString out = nmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("UUID,TYPE,DEVICE,STATE,ACTIVE-PATH,FILENAME"),
                                   QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")},
                                  &ok);
        if (!ok) {
            return;
        }
        for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QStringList f = NetShared::terseFields(line);
            if (f.size() < 6) {
                continue;
            }
            m_active.append(Active{f.at(0), f.at(1), f.at(2), f.at(3), f.at(4), !f.at(5).isEmpty()});
            m_pathToUuid.insert(f.at(4), f.at(0));
        }
    }

    // uuid to {type, D-Bus path}
    QHash<QString, QPair<QString, QString>> readAll()
    {
        QHash<QString, QPair<QString, QString>> all;
        bool ok = false;
        const QString out = nmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("UUID,TYPE,DBUS-PATH"), QStringLiteral("connection"),
                                   QStringLiteral("show")},
                                  &ok);
        if (!ok) {
            return all;
        }
        for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QStringList f = NetShared::terseFields(line);
            if (f.size() >= 3) {
                all.insert(f.at(0), qMakePair(f.at(1), f.at(2)));
            }
        }
        return all;
    }

    void stampDefaults(const QHash<QString, QPair<QString, QString>> &all)
    {
        const int before = m_defaulted.size();
        for (auto it = all.cbegin(); it != all.cend(); ++it) {
            const QString &uuid = it.key();
            const QString &type = it.value().first;
            if ((type != wifiType && type != wiredType) || m_defaulted.contains(uuid)) {
                continue;
            }
            if (connectionFlags(it.value().second) & externalFlag) {
                continue;
            }
            const NetShared::Profile p = NetShared::profile(it.value().second);
            if (p.isEmpty()) {
                continue;
            }
            m_defaulted.insert(uuid);
            const QString method4 = p.value(QStringLiteral("ipv4")).value(QStringLiteral("method")).toString();
            const QString method6 = p.value(QStringLiteral("ipv6")).value(QStringLiteral("method")).toString();
            const bool hotspot = p.value(wifiType).value(QStringLiteral("mode")).toString() == QLatin1String("ap") || method4 == QLatin1String("shared");
            const bool ownServers = !NetShared::profileDnsAddresses(p).isEmpty()
                || p.value(QStringLiteral("ipv4")).value(QStringLiteral("ignore-auto-dns")).toBool();
            if (hotspot || ownServers) {
                continue;
            }
            bool ok = false;
            nmcli(QStringList{QStringLiteral("connection"), QStringLiteral("modify"), uuid}
                      + NetShared::dnsModifyArgs(QStringLiteral("quad9"), {}, NetShared::ipv4TakesDns(method4), NetShared::ipv6TakesDns(method6), true),
                  &ok);
            if (ok) {
                m_reapply.insert(uuid);
            }
        }
        if (m_defaulted.size() != before) {
            QStringList lines(m_defaulted.cbegin(), m_defaulted.cend());
            lines.sort();
            writeFile(defaultedFile, lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
        }

        // A connection being joined right now started with the network's
        // servers; once it is up it takes the new ones in place.
        for (const Active &a : std::as_const(m_active)) {
            if (m_reapply.contains(a.uuid) && a.state == QLatin1String("activated") && !a.device.isEmpty()) {
                m_reapply.remove(a.uuid);
                nmcli({QStringLiteral("device"), QStringLiteral("reapply"), a.device});
            }
        }
        for (auto it = m_reapply.begin(); it != m_reapply.end();) {
            bool joining = false;
            for (const Active &a : std::as_const(m_active)) {
                joining = joining || a.uuid == *it;
            }
            it = joining ? std::next(it) : m_reapply.erase(it);
        }
    }

    void killSwitch(const QHash<QString, QPair<QString, QString>> &all)
    {
        const bool enabled = QFile::exists(killSwitchFile);
        const bool mullvad = QFile::exists(QStringLiteral("/sys/class/net/wg0-mullvad")) || QFile::exists(QStringLiteral("/sys/class/net/wg-mullvad"));

        QHash<QString, Active> up;
        for (const Active &a : std::as_const(m_active)) {
            if (isVpn(a.type) && a.saved && a.state == QLatin1String("activated")) {
                up.insert(a.uuid, a);
            }
        }
        QSet<QString> present;
        for (const Active &a : std::as_const(m_active)) {
            present.insert(a.uuid);
        }

        if (!enabled) {
            m_armed.clear();
            m_userOff.clear();
        } else {
            for (const Active &a : std::as_const(up)) {
                // Its servers are looked up while it is connected, through
                // it, and kept for reconnecting after a drop.
                if (!m_armed.contains(a.uuid) || !m_wasUp.contains(a.uuid)) {
                    m_armed.insert(a.uuid, Armed{a.type, resolveServers(all.value(a.uuid).second)});
                }
            }
            for (auto it = m_armed.begin(); it != m_armed.end();) {
                const QString uuid = it.key();
                const bool keep = up.contains(uuid)
                    || (all.contains(uuid) && it->type != QLatin1String("wireguard") && !m_userOff.contains(uuid));
                if (!keep) {
                    m_userOff.remove(uuid);
                }
                it = keep ? std::next(it) : m_armed.erase(it);
            }
        }
        for (auto it = m_userOff.begin(); it != m_userOff.end();) {
            it = present.contains(*it) || m_armed.contains(*it) ? std::next(it) : m_userOff.erase(it);
        }
        m_wasUp = QSet<QString>(up.keyBegin(), up.keyEnd());

        QStringList blocked;
        for (auto it = m_armed.cbegin(); it != m_armed.cend(); ++it) {
            if (!up.contains(it.key())) {
                blocked.append(it.key());
            }
        }

        QString state = QStringLiteral("off");
        if (enabled) {
            state = mullvad ? QStringLiteral("mullvad") : m_armed.isEmpty() ? QStringLiteral("ready") : blocked.isEmpty() ? QStringLiteral("protecting") : QStringLiteral("blocking");
        }
        applyRules(enabled && !mullvad && !m_armed.isEmpty() ? ruleset(up) : QString());

        QString armed;
        for (auto it = m_armed.cbegin(); it != m_armed.cend(); ++it) {
            armed += it.key() + QLatin1Char(' ') + it->type + QLatin1Char(' ') + it->addresses.join(QLatin1Char(',')) + QLatin1Char('\n');
        }
        const QString report = QStringLiteral("state=%1\nblocked=%2\n").arg(state, blocked.join(QLatin1Char(',')));
        if (armed != m_armedText || report != m_report) {
            writeFile(armedFile, armed);
            writeFile(stateFile, report);
            m_armedText = armed;
            m_report = report;
        }
    }

    QStringList resolveServers(const QString &path)
    {
        QStringList addresses;
        for (const QString &host : NetShared::vpnServers(NetShared::profile(path))) {
            const QHostAddress literal(host);
            if (!literal.isNull()) {
                addresses.append(literal.toString());
                continue;
            }
            for (const QHostAddress &address : QHostInfo::fromName(host).addresses()) {
                addresses.append(address.toString());
            }
        }
        addresses.removeDuplicates();
        return addresses;
    }

    QString ruleset(const QHash<QString, Active> &up) const
    {
        QStringList interfaces{QStringLiteral("wildos-win*")};
        QStringList v4;
        QStringList v6;
        for (auto it = m_armed.cbegin(); it != m_armed.cend(); ++it) {
            if (it->type == QLatin1String("wireguard")) {
                if (up.contains(it.key()) && !up.value(it.key()).device.isEmpty()) {
                    interfaces.append(up.value(it.key()).device);
                }
            } else {
                // NetworkManager's VPN plugins bring up tun or tap devices.
                interfaces << QStringLiteral("tun*") << QStringLiteral("tap*");
            }
            for (const QString &address : it->addresses) {
                (QHostAddress(address).protocol() == QAbstractSocket::IPv6Protocol ? v6 : v4).append(address);
            }
        }
        interfaces.removeDuplicates();

        QString out = QStringLiteral("        oifname \"lo\" accept\n");
        for (const QString &name : std::as_const(interfaces)) {
            out += QStringLiteral("        oifname \"%1\" accept\n").arg(name);
        }
        if (!v4.isEmpty()) {
            out += QStringLiteral("        ip daddr { %1 } accept\n").arg(v4.join(QStringLiteral(", ")));
        }
        if (!v6.isEmpty()) {
            out += QStringLiteral("        ip6 daddr { %1 } accept\n").arg(v6.join(QStringLiteral(", ")));
        }
        out += QStringLiteral("        udp dport 53 reject\n        tcp dport 53 reject\n");
        if (const passwd *resolved = getpwnam("systemd-resolve")) {
            out += QStringLiteral("        meta skuid %1 tcp dport 853 accept\n").arg(resolved->pw_uid);
        }
        out += QStringLiteral(
                   "        udp sport 68 udp dport 67 accept\n"
                   "        udp sport 546 udp dport 547 accept\n"
                   "        icmpv6 type { nd-router-solicit, nd-neighbor-solicit, nd-neighbor-advert, mld2-listener-report } accept\n"
                   "        ip daddr { %1 } accept\n"
                   "        ip6 daddr { %2 } accept\n"
                   "        reject\n")
                   .arg(localV4.join(QStringLiteral(", ")), localV6.join(QStringLiteral(", ")));

        // Windows mode's virtual PC reaches the internet through this one,
        // so it is held to the same rule.
        QString forward = QStringLiteral("        iifname \"wildos-win*\" oifname \"wildos-win*\" accept\n");
        for (const QString &name : std::as_const(interfaces)) {
            if (!name.startsWith(QLatin1String("wildos-win"))) {
                forward += QStringLiteral("        iifname \"wildos-win*\" oifname \"%1\" accept\n").arg(name);
            }
        }
        forward += QStringLiteral("        iifname \"wildos-win*\" ip daddr { %1 } accept\n        iifname \"wildos-win*\" reject\n")
                       .arg(localV4.join(QStringLiteral(", ")));

        return QStringLiteral(
                   "table inet %1\n"
                   "delete table inet %1\n"
                   "table inet %1 {\n"
                   "    chain output {\n"
                   "        type filter hook output priority filter; policy accept;\n"
                   "%2"
                   "    }\n"
                   "    chain forward {\n"
                   "        type filter hook forward priority filter; policy accept;\n"
                   "%3"
                   "    }\n"
                   "}\n")
            .arg(table, out, forward);
    }

    void applyRules(const QString &rules)
    {
        if (rules == m_rules && m_rulesKnown) {
            return;
        }
        bool ok = false;
        if (rules.isEmpty()) {
            // Nothing to remove is fine; the table may never have existed.
            QProcess::execute(QStringLiteral("nft"), {QStringLiteral("delete"), QStringLiteral("table"), QStringLiteral("inet"), table});
            ok = true;
        } else {
            run(QStringLiteral("nft"), {QStringLiteral("-f"), QStringLiteral("-")}, &ok, rules.toUtf8());
        }
        m_rulesKnown = ok;
        m_rules = rules;
    }

    QTimer m_timer;
    QTimer m_poll;
    QFileSystemWatcher m_files;
    QList<Active> m_active;
    QHash<QString, QString> m_pathToUuid;
    QSet<QString> m_defaulted;
    QSet<QString> m_reapply;
    QHash<QString, Armed> m_armed;
    QSet<QString> m_userOff;
    QSet<QString> m_wasUp;
    QString m_rules;
    QString m_armedText;
    QString m_report;
    bool m_rulesKnown = false;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-network"));
    NetworkService service;
    return app.exec();
}

#include "main.moc"
