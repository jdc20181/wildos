#pragma once

// Shared by Settings (network.cpp) and the root service (networkd/main.cpp),
// so both agree on what each DNS choice means and read NetworkManager's
// connection profiles the same way.

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QHostAddress>
#include <QMap>
#include <QMetaType>
#include <QStringList>
#include <QVariantMap>
#include <QtEndian>

namespace NetShared
{

using Profile = QMap<QString, QVariantMap>;

inline const QString nmService = QStringLiteral("org.freedesktop.NetworkManager");

// Every DNS service is reached over TLS, so the server name travels with
// each address in NetworkManager's "address#name" form.
struct DnsProvider {
    const char *id;
    const char *ipv4;
    const char *ipv6;
};

inline constexpr DnsProvider dnsProviders[] = {
    {"quad9", "9.9.9.9#dns.quad9.net,149.112.112.112#dns.quad9.net", "2620:fe::fe#dns.quad9.net,2620:fe::9#dns.quad9.net"},
    {"cloudflare", "1.1.1.1#cloudflare-dns.com,1.0.0.1#cloudflare-dns.com",
     "2606:4700:4700::1111#cloudflare-dns.com,2606:4700:4700::1001#cloudflare-dns.com"},
    {"mullvad", "194.242.2.2#dns.mullvad.net", "2a07:e340::2#dns.mullvad.net"},
};

inline const DnsProvider *provider(const QString &id)
{
    for (const auto &p : dnsProviders) {
        if (id == QLatin1String(p.id)) {
            return &p;
        }
    }
    return nullptr;
}

inline QString serverAddress(const QString &entry)
{
    QString address = entry.section(QLatin1Char('#'), 0, 0).trimmed();
    if (address.startsWith(QLatin1Char('['))) {
        address = address.mid(1).section(QLatin1Char(']'), 0, 0);
    }
    return address;
}

// The choice a profile's DNS servers amount to. A profile with no servers
// of its own that still takes the network's is "automatic".
inline QString dnsChoice(const QStringList &servers, bool ignoreAuto)
{
    if (servers.isEmpty()) {
        return ignoreAuto ? QStringLiteral("custom") : QStringLiteral("automatic");
    }
    for (const auto &p : dnsProviders) {
        const QStringList known = (QString::fromLatin1(p.ipv4) + QLatin1Char(',') + QString::fromLatin1(p.ipv6)).split(QLatin1Char(','));
        for (const QString &k : known) {
            if (servers.contains(serverAddress(k))) {
                return QString::fromLatin1(p.id);
            }
        }
    }
    return QStringLiteral("custom");
}

// The "nmcli connection modify" arguments for a choice. A family whose
// method takes no DNS servers (disabled, link-local, ignore, shared) is left
// alone, since NetworkManager refuses the profile otherwise. Custom servers
// are encrypted only when every one of them names its TLS server.
//
// asDefault is Wild OS's own default for a network nobody has chosen DNS
// for: the provider first and encrypted wherever the network allows, with
// the network's own servers kept behind it. Hotel and café Wi-Fi that hides
// its sign-in page behind its own DNS then still works. A provider someone
// picks in Settings is strict instead: encrypted or nothing.
inline QStringList dnsModifyArgs(const QString &choice, const QStringList &custom, bool ipv4, bool ipv6, bool asDefault = false)
{
    QStringList v4;
    QStringList v6;
    QString tls = QStringLiteral("yes");
    bool ignoreAuto = true;
    if (const DnsProvider *p = provider(choice)) {
        v4 = QString::fromLatin1(p->ipv4).split(QLatin1Char(','));
        v6 = QString::fromLatin1(p->ipv6).split(QLatin1Char(','));
        if (asDefault) {
            tls = QStringLiteral("opportunistic");
            ignoreAuto = false;
        }
    } else if (choice == QLatin1String("custom")) {
        for (const QString &entry : custom) {
            const QHostAddress address(serverAddress(entry));
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                v6.append(entry);
            } else {
                v4.append(entry);
            }
            if (!entry.contains(QLatin1Char('#'))) {
                tls = QStringLiteral("no");
            }
        }
    } else {
        tls = QStringLiteral("default");
        ignoreAuto = false;
    }
    QStringList args;
    if (ipv4) {
        args << QStringLiteral("ipv4.dns") << v4.join(QLatin1Char(','))
             << QStringLiteral("ipv4.ignore-auto-dns") << (ignoreAuto ? QStringLiteral("yes") : QStringLiteral("no"));
    }
    if (ipv6) {
        args << QStringLiteral("ipv6.dns") << v6.join(QLatin1Char(','))
             << QStringLiteral("ipv6.ignore-auto-dns") << (ignoreAuto ? QStringLiteral("yes") : QStringLiteral("no"));
    }
    args << QStringLiteral("connection.dns-over-tls") << tls;
    return args;
}

inline bool ipv4TakesDns(const QString &method)
{
    return method.isEmpty() || method == QLatin1String("auto") || method == QLatin1String("manual");
}

inline bool ipv6TakesDns(const QString &method)
{
    return method.isEmpty() || method == QLatin1String("auto") || method == QLatin1String("dhcp") || method == QLatin1String("manual");
}

// D-Bus hands nested values over as QDBusArgument unless something has
// registered the type, so both shapes are read.
template<typename T>
T unpack(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        T result;
        value.value<QDBusArgument>() >> result;
        return result;
    }
    return qvariant_cast<T>(value);
}

// The profile without its secrets, which is all either side needs.
inline Profile profile(const QString &path)
{
    QDBusMessage call = QDBusMessage::createMethodCall(nmService, path, QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"),
                                                       QStringLiteral("GetSettings"));
    const QDBusMessage reply = QDBusConnection::systemBus().call(call, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return unpack<Profile>(reply.arguments().constFirst());
}

// DNS servers as written in the profile: the text form (with server names)
// where NetworkManager offers it, else the plain address lists.
inline QStringList profileDns(const Profile &p, const QString &family)
{
    const QVariantMap ip = p.value(family);
    const QStringList data = unpack<QStringList>(ip.value(QStringLiteral("dns-data")));
    if (!data.isEmpty()) {
        return data;
    }
    QStringList servers;
    if (family == QLatin1String("ipv4")) {
        for (const uint raw : unpack<QList<uint>>(ip.value(QStringLiteral("dns")))) {
            // Network byte order, as it sits in the message.
            servers.append(QHostAddress(qFromBigEndian<quint32>(raw)).toString());
        }
    } else {
        for (const QByteArray &raw : unpack<QList<QByteArray>>(ip.value(QStringLiteral("dns")))) {
            if (raw.size() == 16) {
                servers.append(QHostAddress(reinterpret_cast<const quint8 *>(raw.constData())).toString());
            }
        }
    }
    return servers;
}

inline QStringList profileDnsAddresses(const Profile &p)
{
    QStringList addresses;
    for (const QString &family : {QStringLiteral("ipv4"), QStringLiteral("ipv6")}) {
        for (const QString &entry : profileDns(p, family)) {
            addresses.append(serverAddress(entry));
        }
    }
    return addresses;
}

// The hosts a VPN connects to: WireGuard's peer endpoints, OpenVPN's
// remotes ("host[:port[:proto]]" or "host port", comma separated).
inline QStringList vpnServers(const Profile &p)
{
    QStringList hosts;
    const auto addHost = [&hosts](QString text) {
        text = text.trimmed();
        if (text.isEmpty()) {
            return;
        }
        QString host;
        if (text.startsWith(QLatin1Char('['))) {
            host = text.mid(1).section(QLatin1Char(']'), 0, 0);
        } else if (text.count(QLatin1Char(':')) > 2) {
            host = text.section(QLatin1Char(' '), 0, 0);
        } else {
            host = text.section(QLatin1Char(' '), 0, 0).section(QLatin1Char(':'), 0, 0);
        }
        if (!host.isEmpty() && !hosts.contains(host)) {
            hosts.append(host);
        }
    };
    const QVariantMap wireguard = p.value(QStringLiteral("wireguard"));
    for (const QVariantMap &peer : unpack<QList<QVariantMap>>(wireguard.value(QStringLiteral("peers")))) {
        addHost(peer.value(QStringLiteral("endpoint")).toString());
    }
    const QVariantMap vpn = p.value(QStringLiteral("vpn"));
    const auto data = unpack<QMap<QString, QString>>(vpn.value(QStringLiteral("data")));
    for (const QString &remote : data.value(QStringLiteral("remote")).split(QLatin1Char(','))) {
        addHost(remote);
    }
    return hosts;
}

// Splits one line of "nmcli -t" output, where a literal colon or backslash
// inside a field is escaped with a backslash.
inline QStringList terseFields(const QString &line)
{
    QStringList fields;
    QString field;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('\\') && i + 1 < line.size()) {
            field.append(line.at(++i));
        } else if (c == QLatin1Char(':')) {
            fields.append(field);
            field.clear();
        } else {
            field.append(c);
        }
    }
    fields.append(field);
    return fields;
}

}
