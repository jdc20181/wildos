#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

// "Sign in with Google" for Gmail, through the Google app that
// evolution-data-server is registered as (Mail is built on its engine, so
// Google's page names Evolution). Google's own sign-in page opens in the
// browser; when the person has signed in, Google sends the browser to a
// com.googleusercontent.apps address that Wild OS hands to Mail
// (org.wildos.mail.desktop), and Mail trades the code in it for a token
// that signs in to IMAP and SMTP. Mail asks for mail and calendars.
class GoogleSignIn : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool waiting READ waiting NOTIFY waitingChanged)

public:
    static GoogleSignIn *instance();
    static GoogleSignIn *create(QQmlEngine *, QJSEngine *);

    bool waiting() const
    {
        return !m_state.isEmpty();
    }

    // Opens Google's page for this address in the browser, for mail and
    // calendars.
    Q_INVOKABLE void start(const QString &email);
    // The same for Calendar, asking for calendars, contacts and tasks
    // instead of mail; the token goes to evolution-data-server.
    Q_INVOKABLE void startCalendar(const QString &email);
    // Opens the same page again, for a closed browser tab.
    Q_INVOKABLE void reopen();
    Q_INVOKABLE void cancel();

    // The address Google sent the browser to, from main.cpp.
    bool handleRedirect(const QString &url);

    // A fresh access token for an account signed in with Google, from its
    // refresh token. Blocks: Camel calls it from its own threads.
    static bool accessToken(const QString &refreshToken, QString *token, int *expiresIn, QString *error);

    static bool isRedirect(const QString &url);

Q_SIGNALS:
    void waitingChanged();
    // The refresh token is what Mail keeps (in KWallet, or its private
    // file) in place of a password.
    void signedIn(const QString &email, const QString &refreshToken);
    void failed(const QString &message);

private:
    explicit GoogleSignIn(QObject *parent = nullptr);
    void begin(const QString &email);
    QByteArray tokenRequest(const QByteArray &body, QString *error) const;

    QString m_email;
    QString m_state;
    QByteArray m_verifier;
    QString m_url;
    QString m_scope;
    QNetworkAccessManager *m_network;
};
