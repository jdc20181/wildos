#pragma once

#include <QJSValue>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QVariantList>

// SSH keys (RemoteKeychain.dc). The keys are ordinary OpenSSH key files in
// ~/.ssh, private ones readable only by their owner, so ssh, scp, git and
// everything else finds them too; Remote only adds a label for each in its
// own data folder. Keys are made with ssh-keygen and loaded into the
// session's ssh-agent with ssh-add. A passphrase is only kept when the
// person asks, in the keyring.
//
// Each key is a map: path, file, label, type ("ED25519", "ED25519-SK",
// "RSA"), bits, fingerprint, publicKey, encrypted (has a passphrase),
// securityKey (a hardware key that must be touched), inAgent,
// passphraseSaved, made (short date in the regional format), usedBy (host names), legacy.
class Keychain : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList keys READ keys NOTIFY changed)
    // An ssh-agent is running in this session (SSH_AUTH_SOCK).
    Q_PROPERTY(bool agent READ agent NOTIFY changed)

public:
    static Keychain *instance();
    static Keychain *create(QQmlEngine *, QJSEngine *);

    QVariantList keys() const { return m_keys; }
    bool agent() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap key(const QString &path) const;
    // The label shown for a private key path, or its file name.
    Q_INVOKABLE QString label(const QString &path) const;

    // callback(ok, message, path)
    Q_INVOKABLE void generate(const QString &label, const QString &passphrase, bool savePassphrase, const QJSValue &callback);
    Q_INVOKABLE void importKey(const QUrl &file, const QJSValue &callback);
    Q_INVOKABLE void exportKey(const QString &path, const QUrl &folder, const QJSValue &callback);
    // Moves the key and its public half to the bin.
    Q_INVOKABLE bool remove(const QString &path);
    Q_INVOKABLE void rename(const QString &path, const QString &label);
    Q_INVOKABLE void copyPublic(const QString &path);
    // Changes or adds a passphrase; asks for the old one if it has one.
    Q_INVOKABLE void changePassphrase(const QString &path, const QString &newPassphrase, const QJSValue &callback);
    // Checks the passphrase opens the key before the keyring gets it.
    Q_INVOKABLE void savePassphrase(const QString &path, const QString &passphrase, const QJSValue &callback);
    Q_INVOKABLE void forgetPassphrase(const QString &path);
    // ssh-add, with the passphrase from the keyring when it is there.
    Q_INVOKABLE void addToAgent(const QString &path, const QJSValue &callback);

    // Before a terminal connects with this key: into the agent, if there is
    // one and the keyring has the passphrase, so ssh doesn't ask.
    void prepare(const QString &path, const std::function<void()> &done);

Q_SIGNALS:
    void changed();

private:
    explicit Keychain(QObject *parent = nullptr);
    void loadAgent(const std::function<void()> &done);
    void writeLabels();
    void call(const QJSValue &callback, const QVariantList &args);

    QVariantList m_keys;
    QVariantMap m_labels;
    QSet<QString> m_agentFingerprints;
};
