#pragma once

#include <QJSValue>
#include <QObject>
#include <QQmlEngine>

#include <functional>

// Passwords and key passphrases in the keyring (KWallet through
// QtKeychain), only ever when the person ticks "Save in my keyring". Nothing
// here writes a secret to a file: when the keyring cannot take it, saving
// fails and Remote asks again next time.
class Secrets : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Secrets *instance();
    static Secrets *create(QQmlEngine *, QJSEngine *);

    void read(const QString &key, const std::function<void(const QString &secret, bool found)> &done);
    void write(const QString &key, const QString &secret, const std::function<void(bool ok)> &done = {});
    Q_INVOKABLE void remove(const QString &key);

    // From QML: callback(secret, found) and callback(ok).
    Q_INVOKABLE void read(const QString &key, const QJSValue &callback);
    Q_INVOKABLE void write(const QString &key, const QString &secret, const QJSValue &callback);

    // Whether a secret is saved, without handing it to QML: callback(found).
    Q_INVOKABLE void exists(const QString &key, const QJSValue &callback);

    // The keyring names for a desktop host's password, an SSH account's
    // password (user@host:port, so the terminal and the files of any host
    // with that account share it, and a host moved to another address
    // never sends it there) and a key's passphrase.
    static QString hostKey(const QString &hostId) { return QStringLiteral("desktop:") + hostId; }
    static QString sshKey(const QString &user, const QString &host, int port)
    {
        return QStringLiteral("ssh:%1@%2:%3").arg(user, host).arg(port);
    }
    static QString passphraseKey(const QString &fingerprint) { return QStringLiteral("key:") + fingerprint; }

private:
    explicit Secrets(QObject *parent = nullptr);
};
