#pragma once

#include "verifier.h"

#include <PolkitQt1/Agent/Listener>
#include <PolkitQt1/Agent/Session>
#include <PolkitQt1/Details>
#include <PolkitQt1/Identity>

#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QVariantList>

// The Wild OS permission prompt: a polkit authentication agent in place of
// KDE's, for the session it runs in. Polkit hands it a request (an action
// id, the identities that may approve it, a cookie); this works out who is
// asking and whether Wild OS vouches for them, then runs the password
// through polkit's own helper. Requests that arrive while one is showing
// wait their turn.
//
// Wrong passwords: three in a row and the prompt waits 30 seconds before
// it takes another; after six polkit is told no and the prompt closes, so
// the app that asked is not left waiting for ever. Messages from the
// password system itself (an account locked after too many tries, an
// expired password) are shown as they come.
//
// Fingerprints: Wild OS's PAM profile (wildos-desktop's
// wildos-fingerprint) takes an empty password as "use my finger" for an
// account with one enrolled. When the chosen person is you and fprintd has
// your finger, the prompt answers polkit's first password question with an
// empty one as soon as it opens, so a touch approves without a key press.
// The first key typed into the field drops that attempt (its helper is
// stopped, nothing is counted as a wrong try) and starts a fresh one for
// the password, so a reader that never answers can't stand in the way.
// pam_fprintd retries a wrong finger itself; once it gives up, or the
// reader fails, the prompt stops listening, and an empty password with Yes
// tries again. A reader that only saw no touch in time is listened to
// again. The reader's own messages come through as plain words.
class Agent : public PolkitQt1::Agent::Listener
{
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY requestChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY requestChanged)
    Q_PROPERTY(QString purpose READ purpose NOTIFY requestChanged)
    // checking, verified or unverified
    Q_PROPERTY(QString trust READ trust NOTIFY requestChanged)
    Q_PROPERTY(QString why READ why NOTIFY requestChanged)
    Q_PROPERTY(QVariantList details READ details NOTIFY requestChanged)
    Q_PROPERTY(QVariantList people READ people NOTIFY requestChanged)
    Q_PROPERTY(int person READ person NOTIFY personChanged)
    Q_PROPERTY(bool forSelf READ forSelf NOTIFY personChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    // What the fingerprint reader wants now ("Touch the fingerprint
    // reader"), not an error.
    Q_PROPERTY(QString info READ info NOTIFY stateChanged)
    // The chosen person is you and has a finger enrolled.
    Q_PROPERTY(bool fingerprint READ fingerprint NOTIFY personChanged)
    // A touch on the reader would approve the request now.
    Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int waitSeconds READ waitSeconds NOTIFY stateChanged)
    Q_PROPERTY(int serial READ serial NOTIFY serialChanged)

public:
    explicit Agent(QObject *parent = nullptr);
    ~Agent() override;

    // Registers with polkit for this session. Retries every few seconds
    // while another agent (KDE's, before the next sign-in) still holds it.
    void start();
    // Fills the prompt with made-up values and shows it, for previews.
    void demo(const QString &kind);

    bool active() const { return m_active; }
    QString appName() const { return m_verdict.appName; }
    QString iconName() const { return m_verdict.iconName; }
    QString purpose() const { return m_verdict.purpose; }
    QString trust() const { return m_trust; }
    QString why() const { return m_verdict.why; }
    QVariantList details() const;
    QVariantList people() const;
    int person() const { return m_person; }
    bool forSelf() const;
    QString error() const { return m_error; }
    QString info() const { return m_info; }
    bool fingerprint() const { return m_fingerprint && forSelf(); }
    bool listening() const { return m_listening; }
    bool busy() const { return m_busy; }
    int waitSeconds() const { return m_waitSeconds; }
    int serial() const { return m_serial; }

    Q_INVOKABLE void choose(int index);
    Q_INVOKABLE void submit(const QString &password);
    Q_INVOKABLE void cancel();
    // Something was typed into the password field: no more listening for a
    // finger on this request.
    Q_INVOKABLE void typing();

public Q_SLOTS:
    void initiateAuthentication(const QString &actionId, const QString &message, const QString &iconName, const PolkitQt1::Details &details,
                                const QString &cookie, const PolkitQt1::Identity::List &identities,
                                PolkitQt1::Agent::AsyncResult *result) override;
    bool initiateAuthenticationFinish() override;
    void cancelAuthentication() override;

Q_SIGNALS:
    void activeChanged();
    void requestChanged();
    void serialChanged();
    void personChanged();
    void stateChanged();

private:
    struct Person {
        uint uid = 0;
        QString login;
        QString fullName;
    };
    struct Request {
        QString actionId;
        QString message;
        QHash<QString, QString> details;
        QString cookie;
        PolkitQt1::Identity::List identities;
        PolkitQt1::Agent::AsyncResult *result = nullptr;
    };

    void begin(const Request &request);
    void startSession();
    void dropSession();
    void onCompleted(bool gained);
    void finish();
    void setError(const QString &text);
    void onMessage(const QString &text, bool error);
    void checkFingerprint();
    void listen();
    void stopListening();

    QQueue<Request> m_queue;
    Request m_request;
    QList<Person> m_people;
    int m_person = 0;
    QPointer<PolkitQt1::Agent::Session> m_session;
    bool m_active = false;
    bool m_cancelled = false;
    bool m_prompted = false;
    bool m_busy = false;
    bool m_demo = false;
    QString m_pending;
    bool m_hasPending = false;
    int m_failures = 0;
    int m_waitSeconds = 0;
    int m_serial = 0;
    QString m_error;
    QString m_info;
    bool m_fingerprint = false;
    enum class FingerEnd { None, TimedOut, NoMatch, Broken };
    // Listen for a finger whenever polkit asks, until something is typed
    // or the reader gives up.
    bool m_autoFinger = false;
    bool m_listening = false;
    bool m_promptEcho = false;
    FingerEnd m_fingerEnd = FingerEnd::None;
    QElapsedTimer m_listenClock;
    QString m_trust = QStringLiteral("checking");
    Verdict m_verdict;
    QTimer m_wait;
    QTimer m_retry;
};
