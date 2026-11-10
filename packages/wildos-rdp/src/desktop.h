#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QObject>
#include <QRect>
#include <QRegion>
#include <QVariantMap>

#include <condition_variable>
#include <mutex>

#include <freerdp/client/disp.h>

class Client;

// A whole remote desktop, the engine's second mode beside Windows mode's
// RemoteApp windows: the same Client (connection, graphics pipeline,
// clipboard, sound, drives, reconnecting), drawn as one picture.
//
// Where Windows mode trusts its own VM without asking, a desktop belongs to
// someone else: the certificate, the sign-in and each reconnection attempt
// are questions for the person. FreeRDP asks them on its own thread, which
// waits here until the GUI thread answers.
class DesktopSession : public QObject
{
    Q_OBJECT

public:
    explicit DesktopSession(QObject *parent = nullptr);
    ~DesktopSession() override;

    Client *client() const { return m_client; }

    // FreeRDP's arguments, as xfreerdp3 takes them. Without a password among
    // them, the sign-in is asked for (credentialsNeeded).
    bool configure(const QList<QByteArray> &args, QString *error);
    // The size of the remote desktop and the scale it draws at; or, for
    // every monitor, their places side by side in physical pixels.
    void setDisplay(const QSize &physical, double scale);
    void setMonitors(const QList<QRect> &monitors, double scale);
    void setReconnectAttempts(int attempts);
    void start();
    void stop();

    // Answers to the questions, from the GUI thread.
    // accept: trust it; remember: keep it in FreeRDP's store so a change is
    // noticed next time (otherwise for this session only).
    void answerCertificate(bool accept, bool remember);
    void answerCredentials(bool ok, const QString &user, const QString &domain, const QString &password);
    // Reconnecting: "Try now" skips the wait; "Stop trying" gives up.
    void tryNow();
    void stopTrying();

    // The desktop's picture as last drawn, and the round trip Windows
    // measured (0 when it hasn't said).
    QImage frame() const;
    QSize desktopSize() const;
    int latency() const;

    // Asks Windows to lay the desktop out for a new window size (dynamic
    // resolution); false when it can't, and the picture is scaled instead.
    bool resize(const QSize &physical, double scale);

Q_SIGNALS:
    // info: host, port, commonName, subject, issuer, fingerprint (SHA-256,
    // colon separated), validUntil (short date in the regional format), expired, selfSigned,
    // mismatch; with `changed`, also previousFingerprint.
    void certificateNeeded(const QVariantMap &info, bool changed);
    // reason: "first" when nothing was given, "wrong" after a refusal.
    void credentialsNeeded(const QString &user, const QString &domain, const QString &reason);
    void connected();
    void reconnecting(int attempt, int attempts);
    void reconnected();
    // The session ended: `code` is FreeRDP's name for why (empty when it
    // ended normally or was ended here); `authFailed` when Windows refused
    // the account.
    void finished(const QString &code, bool authFailed);
    void frameChanged(const QRegion &damage);
    void pointerImage(const QImage &image, const QPoint &hotspot);
    void pointerHidden();
    void pointerDefault();

private:
    friend struct DesktopCallbacks;
    void wake();

    Client *m_client;
    mutable std::mutex m_lock;
    std::condition_variable m_wait;
    // The answer FreeRDP's thread is waiting for.
    int m_certificateAnswer = -1;
    bool m_credentialsAnswered = false;
    bool m_credentialsOk = false;
    QByteArray m_user;
    QByteArray m_domain;
    QByteArray m_password;
    bool m_tryNow = false;
    bool m_stopTrying = false;
    bool m_stopping = false;
    bool m_passwordGiven = false;
    int m_attempts = 5;
    DispClientContext *m_disp = nullptr;
};
