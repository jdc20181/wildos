#pragma once

#include <QJSValue>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

// Is this the same server as last time? Host identities for SSH, kept in
// ~/.ssh/known_hosts like every other SSH program on the PC uses them.
//
// Before a terminal connects, Remote asks the server for its keys
// (ssh-keyscan) and looks them up (ssh-keygen -F, which understands hashed
// entries and the system's own list). A key seen before connects; a host
// never seen asks first; a host whose key changed gets the warning
// (RemoteHostChanged.dc) and nothing is changed unless the person says the
// server was rebuilt. ssh itself then runs with StrictHostKeyChecking=yes,
// so whatever the scan saw, the connection only goes ahead with a key in
// known_hosts: nothing is ever accepted on the way.
class HostKeys : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList known READ known NOTIFY knownChanged)

public:
    static HostKeys *instance();
    static HostKeys *create(QQmlEngine *, QJSEngine *);

    // callback({ state: "known" | "new" | "changed" | "revoked" | "error",
    //   name, keyType, fingerprint, keyLine,
    //   previous: [{ keyType, fingerprint, seen }], error })
    Q_INVOKABLE void check(const QString &host, int port, const QJSValue &callback);
    // Adds the key a check found, in the form the system asks for (hashed
    // names, Debian's default).
    Q_INVOKABLE bool trust(const QString &host, int port, const QString &keyLine);
    // The server was rebuilt: every old key for it goes, the new one is
    // added.
    Q_INVOKABLE bool replace(const QString &host, int port, const QString &keyLine);

    // The entries of ~/.ssh/known_hosts for the Known hosts tab: [{ index,
    // name, hashed, keyType, fingerprint, seen }]. A hashed name is shown
    // as the saved host it matches, if any.
    QVariantList known();
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool forget(int index);

    static QString knownHostsFile();
    static QString entryName(const QString &host, int port);
    // SHA256:... for a key line or a public key file's text.
    static QString fingerprint(const QString &keyLine);

Q_SIGNALS:
    void knownChanged();

private:
    explicit HostKeys(QObject *parent = nullptr);
    void remember(const QString &fingerprint);
    QString seen(const QString &fingerprint) const;
    bool append(const QString &line);

    QVariantList m_known;
    bool m_loaded = false;
};
