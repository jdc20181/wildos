#pragma once

#include <QObject>
#include <QString>

class Rail;
class Client;

// org.wildos.RemoteApp on the session bus, for the Windows mode agent:
// it shows a loading window the moment an app is asked for, tells the
// windows which app they belong to, and hears what happened to both.
class Service : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.RemoteApp")

public:
    Service(Rail *rail, Client *client, QObject *parent = nullptr);
    bool registerOnBus();

    void setConnected(bool connected) { m_connected = connected; }

public Q_SLOTS:
    // Shows the loading window for an app; returns a token for it. With a
    // delay it only shows if the app has not opened by then (board 9.12:
    // no loader for an app that opens in under half a second).
    // `program` is the app's program as Windows names it (a path or file
    // name), which tells its windows from every other.
    uint Prepare(const QString &appId, const QString &title, const QString &icon, const QString &program, uint delayMs);
    // Stage "almost" once the app's process is up; a reason in plain words
    // for board 9.13, with percent -1 when there is nothing to measure.
    bool Update(uint token, const QString &stage, const QString &reason, int percent, const QString &progress);
    // Closes a loading window that is still waiting; false if it is not.
    bool Cancel(uint token);
    // The app a Windows window belongs to (its taskbar group).
    void SetWindowApp(qulonglong windowId, const QString &appId);
    // Closes a Windows window as its own close button does (Windows asks
    // about unsaved work first, as it always does).
    void CloseWindow(qulonglong windowId);
    // Ends the session.
    void Disconnect();
    // {"connected": bool, "windows": [{"id": n, "app": s, "visible": b}]}
    QString Status();
    // Where each app's window was last, for the org.wildos.rdp.place KWin
    // script: {"wildos-winmode-word": {"x": n, "y": n}}.
    QString Placements();
    // Doubles: the KWin script's numbers are JavaScript's, and D-Bus only
    // delivers a call whose argument types match exactly.
    void SavePlacement(const QString &appId, double x, double y);

Q_SIGNALS:
    void WindowAdded(qulonglong windowId, const QString &title);
    void WindowRemoved(qulonglong windowId);
    void LoadingShown(uint token, uint milliseconds);
    void LoadingFinished(uint token, const QString &reason, qulonglong windowId);
    // "Show details" on a loading window that is taking longer.
    void DetailsRequested(uint token);
    // Try again on a loading window whose app ended before it opened.
    void RetryRequested(uint token);
    // A window closed here that Windows did not close, with nothing unsaved
    // in it: the agent ends its program (`pid`, image `program`).
    void CloseStalled(qulonglong windowId, uint pid, const QString &program);
    void Connected();
    void Disconnected(const QString &error, bool authFailed);

private:
    Rail *m_rail;
    Client *m_client;
    bool m_connected = false;
};
