#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>

namespace SessionInfo
{
// The account's graphical sign-in session in logind, as an id and an object
// path. A systemd user service is outside every session, so this asks
// logind for the account's display session when XDG_SESSION_ID is unset.
QString id();
QDBusObjectPath path();
}

// org.wildos.SystemDialogs on the session bus, served by the long-running
// "wildos-dialogs --session". Ctrl+Alt+Del and Settings start a short-lived
// "wildos-dialogs security" or "restart", which asks this to show the
// screen that is already loaded, so it appears at once.
//
// Windows mode (package wildos-winmode) asks two questions through it and
// waits for the answer: whether to keep Windows apps' work when the
// session ends (board 9.9), and whether a call app may have the camera and
// microphone (board 9.10).
class DialogsService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.SystemDialogs")

public:
    using QObject::QObject;

    static constexpr const char *serviceName = "org.wildos.SystemDialogs";
    static constexpr const char *objectPath = "/Dialogs";

    // Asks the running service; false when there is none or it did not
    // answer within a second, so the caller can show the screen itself.
    static bool forward(const QString &method);

public Q_SLOTS:
    Q_SCRIPTABLE void ShowSecurity();
    Q_SCRIPTABLE void ShowRestart();
    // action: shutdown, restart or signout; appsJson: [{app, document,
    // unsaved}]. Answers keep, discard or back.
    Q_SCRIPTABLE QString AskWindowsShutdown(const QString &action, const QString &appsJson);
    // devicesJson: {cameras: [name], microphones: [{id, name}]}. Answers
    // {"choice": "allow"|"deny", "devices": {camera, microphone}}.
    Q_SCRIPTABLE QString AskWindowsDevices(const QString &appName, const QString &devicesJson);
    // A Windows app was opened while Windows is off or asleep: a card with
    // a bar that fills over about `seconds` until Windows mode hides it
    // (the app's window is up, or the launch failed). cold: Windows starts
    // from off rather than waking.
    Q_SCRIPTABLE void ShowWindowsStarting(const QString &appName, const QString &iconName, int seconds, bool cold);
    Q_SCRIPTABLE void HideWindowsStarting();

Q_SIGNALS:
    void securityRequested();
    void restartRequested();
    void windowsShutdownRequested(const QString &action, const QString &appsJson, const QDBusMessage &reply);
    void windowsDevicesRequested(const QString &appName, const QString &devicesJson, const QDBusMessage &reply);
    void windowsStartingRequested(const QString &appName, const QString &iconName, int seconds, bool cold);
    void windowsStartingDone();
};
