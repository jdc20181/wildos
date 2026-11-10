#pragma once

#include <QHash>
#include <QDateTime>
#include <QMutex>
#include <QSet>
#include <QString>

#include <camel/camel.h>

// The one CamelSession Mail runs on. Camel asks it for passwords and whether
// to trust a server's certificate; everything else (connecting, syncing,
// caching for offline use) Camel does itself. Its services are named by the
// account id, with ".smtp" for the outgoing side.
class Session
{
public:
    static Session &instance();

    CamelSession *camel() const
    {
        return m_session;
    }

    // Called from the GUI thread before a service connects; read from
    // Camel's worker threads.
    void setPassword(const QString &accountId, const QString &password);
    QString password(const QString &accountId) const;
    // Accounts signed in with Google: their stored secret is a refresh
    // token, traded for short-lived access tokens as Camel needs them.
    void setOAuth(const QString &accountId, bool oauth);
    bool oauth(const QString &accountId) const;
    bool accessToken(const QString &accountId, QString *token, int *expiresIn, QString *error);

    // The account a service belongs to: "abc" for both "abc" and "abc.smtp".
    static QString accountOf(CamelService *service);

private:
    Session();

    CamelSession *m_session = nullptr;
    mutable QMutex m_lock;
    QHash<QString, QString> m_passwords;
    QSet<QString> m_oauth;
    struct Token {
        QString value;
        QDateTime expires;
    };
    QHash<QString, Token> m_tokens;
};
