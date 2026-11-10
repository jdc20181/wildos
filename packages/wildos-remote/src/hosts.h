#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

// The saved hosts, desktop (Remote Desktop) and terminal (SSH) alike, with
// their groups. Kept in ~/.config/wildos-remote/hosts.json, which never
// holds a password: those go to the keyring (Secrets) and only when the
// person ticks it. Screens from the end of a desktop session are kept in
// the cache folder, on this PC only.
//
// A host is a map:
//   id, kind ("desktop" or "terminal"), name, address (host or host:port),
//   user, group, note, favourite, lastUsed (ISO date),
//   desktop: { display, resolution, scaling, password ("ask" or
//              "keyring"), clipboard, sound, microphone, printers,
//              folders, folderPath, gateway, speed }
//   ssh: { identity (a private key path, or empty for ssh's own choice),
//          forwards: [{ localPort, remoteHost, remotePort }], colour }
class Hosts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList hosts READ hosts NOTIFY changed)
    Q_PROPERTY(QStringList groups READ groups NOTIFY changed)
    Q_PROPERTY(QVariantMap reach READ reach NOTIFY reachChanged)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static Hosts *instance();
    static Hosts *create(QQmlEngine *, QJSEngine *);

    QVariantList hosts() const { return m_hosts; }
    QStringList groups() const { return m_groups; }
    QVariantMap reach() const { return m_reach; }
    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantMap host(const QString &id) const;
    // Adds the host when its id is new or empty; returns the id.
    Q_INVOKABLE QString save(const QVariantMap &host);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void setFavourite(const QString &id, bool favourite);
    Q_INVOKABLE void touch(const QString &id);
    Q_INVOKABLE void addGroup(const QString &name);
    Q_INVOKABLE void renameGroup(const QString &from, const QString &to);
    Q_INVOKABLE void removeGroup(const QString &name);

    // What the search box can connect to as typed: "ssh alex@web-01 -p
    // 2222", "alex@web-01", "rdp://pc" or "pc:3389". An empty map when the
    // text is a search, not an address.
    Q_INVOKABLE QVariantMap parseAddress(const QString &text) const;
    // A host's address split into host and port, with the kind's default
    // port when none is given.
    Q_INVOKABLE QString hostName(const QVariantMap &host) const;
    Q_INVOKABLE int port(const QVariantMap &host) const;
    // An address as the host settings show it: {host, port}, the port 0
    // when none is given. joinAddress puts them back together, leaving out
    // the kind's default port, which is how hosts store it.
    Q_INVOKABLE QVariantMap addressParts(const QString &address) const;
    Q_INVOKABLE QString joinAddress(const QString &host, int port, const QString &kind) const;
    // Where a terminal host's SSH password is kept in the keyring (Secrets),
    // or empty for a desktop. A host that is deleted, or moved to another
    // address or account, takes its saved password with it unless another
    // host still uses that account.
    Q_INVOKABLE QString passwordKey(const QVariantMap &host) const;

    // Whether each host answers on its port, for Online and Offline on the
    // recent desktops. Only opens and closes a TCP connection.
    Q_INVOKABLE void probe(const QString &id);
    // The screen at the end of the last session, or an empty URL.
    Q_INVOKABLE QUrl screenUrl(const QString &id) const;
    QString screenPath(const QString &id) const;
    void screenSaved(const QString &id);

    // The colours a terminal host's tile can take (RemoteHosts.dc).
    Q_INVOKABLE QStringList colours() const;

Q_SIGNALS:
    void changed();
    void reachChanged();

private:
    explicit Hosts(QObject *parent = nullptr);
    void load();
    void write();
    int indexOf(const QString &id) const;
    void forgetUnused(const QString &key);

    QVariantList m_hosts;
    QStringList m_groups;
    QVariantMap m_reach;
    QHash<QString, qint64> m_screens;
    int m_revision = 0;
};
