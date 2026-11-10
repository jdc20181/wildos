#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

// The people on this computer, after Windows 11's Accounts pages.
//
// Your own name, picture and password hint go through AccountsService,
// which lets an account change its own details without a password. Your
// password goes through wildos-wallet-keys (in wildos-setup), which checks
// the current one and changes it as root through PAM's passwd rules: the
// hardened system keeps passwd without setuid, so an account cannot run
// it on itself, and AccountsService demands an administrator for any
// password change. Adding, removing and promoting
// people and automatic sign-in need root and go through
// /usr/libexec/wildos/settings-users under pkexec, which also puts every
// new account in the groups Wild OS needs (console above all, or the
// console lockdown refuses to let them sign in).
//
// Your wallet (KWallet, where apps keep passwords) opens at sign-in with a
// key pam_kwallet makes from your password. Turning automatic sign-in on
// takes your password, so the system can keep that key and open the wallet
// when no password is typed (wildos-wallet-keys, in wildos-setup). A
// password change re-encrypts the wallet with the key from the new
// password and refreshes the kept key, so all three keep agreeing.
class Users : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap me READ me NOTIFY changed)
    Q_PROPERTY(QVariantList others READ others NOTIFY changed)
    Q_PROPERTY(QString autoLogin READ autoLogin NOTIFY changed)
    Q_PROPERTY(int adminCount READ adminCount NOTIFY changed)
    Q_PROPERTY(bool accountsService READ accountsService NOTIFY changed)
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)

public:
    explicit Users(QObject *parent = nullptr);

    QVariantMap me() const { return m_me; }
    QVariantList others() const { return m_others; }
    QString autoLogin() const { return m_autoLogin; }
    int adminCount() const { return m_adminCount; }
    bool accountsService() const { return m_accountsService; }
    QString busy() const { return m_busy; }

    Q_INVOKABLE void setRealName(const QString &name);
    // Crops the square at (x, y) of side `size`, in the picture's own
    // pixels, and makes it the account picture.
    Q_INVOKABLE void setPicture(const QString &file, qreal x, qreal y, qreal size);
    Q_INVOKABLE void clearPicture();
    Q_INVOKABLE QSize pictureSize(const QString &file) const;
    Q_INVOKABLE void changePassword(const QString &current, const QString &next, const QString &hint);
    Q_INVOKABLE void addUser(const QString &fullName, const QString &userName, const QString &password, bool admin);
    Q_INVOKABLE void removeUser(const QString &userName);
    Q_INVOKABLE void setAdmin(const QString &userName, bool admin);
    // Turning it on needs your own password, for the wallet key.
    Q_INVOKABLE void setAutoLogin(bool on, const QString &password = QString());
    // Gets your saved passwords opening by themselves again: checks your
    // account password, keeps the automatic sign-in key when that is on,
    // and moves the wallet to your account password, from walletPassword
    // when it uses another one (fails with "wallet-other" until given it).
    Q_INVOKABLE void openWallet(const QString &password, const QString &walletPassword = QString());

    Q_INVOKABLE QString suggestUserName(const QString &fullName) const;
    // Empty when the name can be used, otherwise why not.
    Q_INVOKABLE QString checkUserName(const QString &userName) const;

public Q_SLOTS:
    void reload();

Q_SIGNALS:
    void changed();
    void busyChanged();
    void done(const QString &what, const QString &message);
    void failed(const QString &what, const QString &reason);

private:
    void setBusy(const QString &busy);
    void helper(const QString &what, const QStringList &arguments, const QByteArray &input, const QString &success);
    void callUser(const QString &method, const QVariant &argument, const QString &what);
    QString userPath() const;

    QVariantMap m_me;
    QVariantList m_others;
    QString m_autoLogin;
    int m_adminCount = 0;
    bool m_accountsService = false;
    QString m_busy;
};
