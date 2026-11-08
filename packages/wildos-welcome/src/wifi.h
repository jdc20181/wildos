#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

#include <functional>

/* NetworkManager for the setup session's Wi-Fi step, through nmcli. The same
 * logic the installer's Wi-Fi step used.
 *
 * The setup session runs as the wildos-setup account, which polkit lets
 * scan, turn Wi-Fi on and add system connections (10-wildos-setup.rules in
 * wildos-setup). A system connection belongs to the computer, not to the
 * setup account, so the new account is online from its first sign-in.
 * Device state is polled every few seconds; the network list only while the
 * Wi-Fi page asks for it (see active).
 */
class Wifi : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use FirstBoot.wifi")

    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool wiredOnline READ wiredOnline NOTIFY changed)
    Q_PROPERTY(bool online READ online NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY changed)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY changed)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit Wifi(QObject *parent = nullptr);

    bool ready() const { return m_ready; }
    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    bool wiredOnline() const { return m_wiredOnline; }
    bool online() const { return m_online; }
    bool scanning() const { return m_scanning; }
    QString connectedSsid() const { return m_connectedSsid; }
    QString connectingSsid() const { return m_connectingSsid; }
    QVariantList networks() const { return m_networks; }
    bool active() const { return m_active; }
    void setActive(bool active);

    Q_INVOKABLE void rescan();
    /// An empty @p password uses a saved connection or joins an open network.
    Q_INVOKABLE void connectTo(const QString &ssid, const QString &password);
    /// A network that does not announce its name; an empty password is an open one.
    Q_INVOKABLE void connectHidden(const QString &ssid, const QString &password);
    Q_INVOKABLE void setRadio(bool on);
    /// Connect automatically, as the Wi-Fi step's tick box sets it.
    Q_INVOKABLE void setAutoConnect(const QString &ssid, bool on);

    /// Splits one line of `nmcli -t -e yes` output into its fields.
    static QStringList splitTerse(const QString &line);

Q_SIGNALS:
    void changed();
    void networksChanged();
    void activeChanged();
    /// @p reason is "" on success, else "password", "range" or "other".
    void connectFinished(const QString &ssid, bool ok, const QString &reason);

private:
    using Done = std::function<void(int exitCode, const QString &out, const QString &err)>;
    void run(const QStringList &args, Done done, int timeoutMs = 15000, const QByteArray &input = {});
    void finishConnect(const QString &ssid, int code, const QString &text, bool madeProfile);
    void addProfile(const QString &ssid, const QString &password, const QString &keyMgmt, bool hidden);
    void poll();
    void readDevices(const QString &out);
    void listNetworks();

    QTimer m_timer;
    bool m_polling = false;
    bool m_ready = false;
    bool m_available = false;
    bool m_enabled = false;
    bool m_wiredOnline = false;
    bool m_online = false;
    bool m_scanning = false;
    bool m_active = false;
    QString m_connectedSsid;
    QString m_connectingSsid;
    QStringList m_saved;
    QVariantList m_networks;
};
