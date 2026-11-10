#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

class DesktopSession;

// One Remote Desktop connection from Remote (RemoteCheck.dc to
// RemoteWindowed.dc): its host's settings turned into FreeRDP's, the
// questions it asks on the way in, the session while it runs, and why it
// ended.
//
// state: "connecting", "certificate", "signin", "connected",
// "reconnecting" or "ended".
class RemoteDesktop : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString hostId READ hostId CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString user READ user NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool everConnected READ everConnected NOTIFY stateChanged)
    Q_PROPERTY(int attempt READ attempt NOTIFY stateChanged)
    Q_PROPERTY(int attempts READ attempts CONSTANT)
    Q_PROPERTY(int latency READ latency NOTIFY latencyChanged)
    Q_PROPERTY(QString endTitle READ endTitle NOTIFY stateChanged)
    Q_PROPERTY(QString endText READ endText NOTIFY stateChanged)
    Q_PROPERTY(QString display READ display WRITE setDisplay NOTIFY displayChanged)
    Q_PROPERTY(bool multiMonitor READ multiMonitor CONSTANT)
    // With all monitors: [{ screen (its name), rect (its part of the
    // desktop) }], as the desktop was laid out.
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY stateChanged)
    Q_PROPERTY(QSize desktopSize READ desktopSize NOTIFY desktopSizeChanged)
    Q_PROPERTY(bool clipboard READ clipboard WRITE setClipboard NOTIFY sharingChanged)
    Q_PROPERTY(QVariantMap sharing READ sharing NOTIFY sharingChanged)
    Q_PROPERTY(bool sharingChanged READ sharingPending NOTIFY sharingChanged)
    Q_PROPERTY(bool sendShortcuts READ sendShortcuts WRITE setSendShortcuts NOTIFY sendShortcutsChanged)
    Q_PROPERTY(QObject *session READ sessionObject CONSTANT)

public:
    RemoteDesktop(const QVariantMap &host, const QString &display, QObject *parent = nullptr);
    ~RemoteDesktop() override;

    QString hostId() const { return m_host.value(QStringLiteral("id")).toString(); }
    QString name() const { return m_host.value(QStringLiteral("name")).toString(); }
    QString address() const { return m_host.value(QStringLiteral("address")).toString(); }
    QString user() const { return m_user; }
    QString state() const { return m_state; }
    bool everConnected() const { return m_everConnected; }
    int attempt() const { return m_attempt; }
    int attempts() const { return 5; }
    int latency() const { return m_latency; }
    QString endTitle() const { return m_endTitle; }
    QString endText() const { return m_endText; }
    QString display() const { return m_display; }
    void setDisplay(const QString &display);
    bool multiMonitor() const { return m_monitors.size() > 1; }
    QVariantList monitors() const { return m_monitors; }
    QSize desktopSize() const;
    bool clipboard() const { return m_clipboard; }
    void setClipboard(bool on);
    QVariantMap sharing() const { return m_sharing; }
    bool sharingPending() const { return m_sharingPending; }
    bool sendShortcuts() const { return m_sendShortcuts; }
    void setSendShortcuts(bool on);
    DesktopSession *session() const { return m_session; }
    QObject *sessionObject() const;

    // Begins connecting; a password from the keyring comes along if the
    // host keeps one there.
    void start(const QString &password);

    Q_INVOKABLE void answerCertificate(bool accept, bool remember);
    Q_INVOKABLE void signIn(const QString &user, const QString &password, bool save);
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void tryNow();
    Q_INVOKABLE void stopTrying();
    Q_INVOKABLE void disconnectNow();
    // "ctrl-alt-del", "windows", "print", "alt-tab"
    Q_INVOKABLE void sendKeys(const QString &which);
    // A sharing switch from the toolbar, for when it reconnects.
    Q_INVOKABLE void setSharing(const QString &key, bool on);
    // The screen as it ends, for the start page (kept on this PC only).
    void saveScreen();

Q_SIGNALS:
    void stateChanged();
    void latencyChanged();
    void displayChanged();
    void desktopSizeChanged();
    void sharingChanged();
    void sendShortcutsChanged();
    void certificateNeeded(const QVariantMap &info, bool changed);
    void credentialsNeeded(const QString &user, const QString &reason);
    // Signed in: the window opens.
    void opened();
    // Ended after being open, by the other end: a notification if the
    // window isn't in front.
    void lost();
    // The wrong password: ask again, on a fresh connection.
    void retryWanted(const QString &user);

private:
    QList<QByteArray> arguments(const QString &password) const;
    void finished(const QString &code, bool authFailed);
    void setState(const QString &state);

    QVariantMap m_host;
    QString m_display;
    QVariantList m_monitors;
    QString m_user;
    QString m_state = QStringLiteral("connecting");
    bool m_everConnected = false;
    bool m_savePassword = false;
    QString m_pendingPassword;
    int m_attempt = 0;
    int m_latency = 0;
    QString m_endTitle;
    QString m_endText;
    bool m_clipboard = true;
    QVariantMap m_sharing;
    bool m_sharingPending = false;
    bool m_sendShortcuts = false;
    DesktopSession *m_session = nullptr;
    QTimer m_latencyTimer;
    QImage m_lastFrame;
};

// Starting desktops from QML, and the ones open now.
class Desktops : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Desktops *instance();
    static Desktops *create(QQmlEngine *, QJSEngine *);

    // display: "window", "full", "all", or empty for the host's own choice.
    Q_INVOKABLE RemoteDesktop *start(const QVariantMap &host, const QString &display);
    // A window of a new size: Windows lays the desktop out for it when it
    // can (dynamic resolution), otherwise the picture is scaled.
    Q_INVOKABLE bool resizeTo(RemoteDesktop *desktop, const QSize &physical, double scale);
    // From Windows mode: FreeRDP's arguments as they come for xfreerdp3.
    RemoteDesktop *startWithArguments(const QString &title, const QList<QByteArray> &args);

    // Ends every session cleanly (on quit).
    void closeAll();

Q_SIGNALS:
    // A RemoteDesktop to show questions for and then a window.
    void started(RemoteDesktop *desktop);

private:
    explicit Desktops(QObject *parent = nullptr);
    QList<QPointer<RemoteDesktop>> m_open;
};
