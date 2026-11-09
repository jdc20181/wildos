#pragma once

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QTimer;

// Settings > Network and internet > Advanced network settings
// (SettingsNetwork.dc): VPNs and the kill switch, saved Wi-Fi networks, the
// current network's address, DNS, private hardware address and metering,
// and the proxy.
//
// Lists, state and switching connections on and off go through
// NetworkManagerQt. Profiles are read whole from NetworkManager (the parts
// NetworkManagerQt does not model, such as DNS server names, would be lost
// on a round trip through it) and changed with nmcli, which knows every
// setting of the NetworkManager it ships with and leaves the rest of the
// profile, saved passwords included, as it was. The kill switch belongs to
// the root service wildos-network; Settings only turns it on or off through
// the settings-network helper and reads what it reports.
class NetworkSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{uuid, name, kind, server, state ("on", "connecting", "dropped",
    // "off"), since (ms since epoch, 0 if unknown)}]
    Q_PROPERTY(QVariantList vpns READ vpns NOTIFY vpnsChanged)
    // [{uuid, name, active, autoconnect, metered, lastUsed (ms, 0 never)}],
    // the network in use first, then the most recently used.
    Q_PROPERTY(QVariantList savedWifi READ savedWifi NOTIFY savedWifiChanged)
    // The Wi-Fi or Ethernet connection in use, or empty: uuid, name, wifi,
    // address, prefix, gateway, manual, dns, dnsServers, dnsCustom,
    // dnsEncrypted, privateAddress, metered, speed (kbit/s), frequency (MHz),
    // ipv6 (list), mac, permanentMac, interface.
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(bool killSwitch READ killSwitch NOTIFY killSwitchChanged)
    // "" (off or nothing to guard), "protecting", "blocking" (a VPN dropped
    // and the internet is held back) or "mullvad" (the Mullvad app's own
    // tunnel is up and its kill switch is in charge).
    Q_PROPERTY(QString killSwitchState READ killSwitchState NOTIFY killSwitchChanged)
    // What is being changed right now: "dns", "address", "private",
    // "metered", "killswitch", "import", "vpn:<uuid>", "forget:<uuid>".
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)
    // {mode ("off", "automatic", "script", "manual"), host, port, script}
    Q_PROPERTY(QVariantMap proxy READ proxy NOTIFY proxyChanged)

public:
    explicit NetworkSettings(QObject *parent = nullptr);

    QVariantList vpns() const { return m_vpns; }
    QVariantList savedWifi() const { return m_savedWifi; }
    QVariantMap current() const { return m_current; }
    bool killSwitch() const { return m_killSwitch; }
    QString killSwitchState() const { return m_killSwitchState; }
    QString busy() const { return m_busy; }
    QVariantMap proxy() const { return m_proxy; }

    Q_INVOKABLE void refresh();

    Q_INVOKABLE void setVpnActive(const QString &uuid, bool on);
    // A WireGuard .conf or an OpenVPN .ovpn file.
    Q_INVOKABLE void importVpn(const QUrl &file);
    // {name, privateKey, address, dns, publicKey, endpoint}
    Q_INVOKABLE void addWireGuard(const QVariantMap &details);
    // A sentence saying what is wrong with typed WireGuard details, or "".
    Q_INVOKABLE QString checkWireGuard(const QVariantMap &details) const;
    Q_INVOKABLE void setKillSwitch(bool on);

    Q_INVOKABLE void forget(const QString &uuid);

    // choice: automatic, quad9, cloudflare, mullvad or custom (servers
    // separated by commas or spaces, each optionally "address#tls-name").
    Q_INVOKABLE void setDns(const QString &choice, const QString &custom = QString());
    Q_INVOKABLE QString checkCustomDns(const QString &custom) const;
    Q_INVOKABLE void setAddress(bool automatic, const QString &address = QString(), const QString &mask = QString(),
                                const QString &gateway = QString());
    // A sentence saying what is wrong with a manual address, or "".
    Q_INVOKABLE QString checkAddress(const QString &address, const QString &mask, const QString &gateway) const;
    Q_INVOKABLE void setPrivateAddress(bool on);
    Q_INVOKABLE void setMetered(bool on);

    Q_INVOKABLE void setProxy(const QString &mode, const QString &host = QString(), int port = 0, const QString &script = QString());

Q_SIGNALS:
    // Each list says when it really changed, so a repeater over one is not
    // rebuilt (losing focus and hover) because the Wi-Fi speed moved.
    void vpnsChanged();
    void savedWifiChanged();
    void currentChanged();
    void killSwitchChanged();
    void busyChanged();
    void proxyChanged();
    void done(const QString &title, const QString &message);
    void failed(const QString &title, const QString &reason);

private Q_SLOTS:
    void scheduleRefresh();
    void profileChanged();

private:
    using Finished = std::function<void(bool ok, const QString &output)>;

    void nmcli(const QStringList &args, const Finished &finished);
    void setBusy(const QString &busy);
    // Changes the current connection's profile, then puts the change to
    // work: reapplied in place where NetworkManager can, else by
    // reconnecting.
    void modifyCurrent(const QString &busyKey, const QStringList &settings, bool reconnect, const QString &failTitle);
    void importFile(const QString &path, const QString &type, const QString &name);
    QVariantMap readCurrent();
    void readKillSwitch();
    void readProxy();

    QVariantList m_vpns;
    QVariantList m_savedWifi;
    QVariantMap m_current;
    QVariantMap m_proxy;
    bool m_killSwitch = false;
    QString m_killSwitchState;
    QString m_busy;
    QTimer *m_refreshTimer = nullptr;
    QMap<QString, QMap<QString, QVariantMap>> m_profiles;
    // When this session saw each VPN come up, for "Connected 2 h 14 min".
    QHash<QString, qint64> m_vpnSince;
    QHash<QString, uint> m_flags;
    // VPNs the kill switch is holding the internet back for.
    QStringList m_blocked;
    bool m_firstRefresh = true;
};
