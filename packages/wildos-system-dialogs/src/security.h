#pragma once

#include <QObject>
#include <QString>

// What the Ctrl+Alt+Del screen does. Every action goes straight to the
// service that owns it (logind, the display manager, the session manager)
// rather than through the desktop shell, so it still works while the shell
// or an app has frozen the desktop.
class Security : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool canSwitchUser READ canSwitchUser NOTIFY refreshed)
    Q_PROPERTY(bool canSignOut READ canSignOut NOTIFY refreshed)
    Q_PROPERTY(bool canChangePassword READ canChangePassword NOTIFY refreshed)
    Q_PROPERTY(bool canTaskManager READ canTaskManager NOTIFY refreshed)
    Q_PROPERTY(bool canSleep READ canSleep NOTIFY refreshed)
    Q_PROPERTY(bool canShutDown READ canShutDown NOTIFY refreshed)
    Q_PROPERTY(bool networkAvailable READ networkAvailable NOTIFY refreshed)
    Q_PROPERTY(QString networkText READ networkText NOTIFY refreshed)
    Q_PROPERTY(bool networkConnected READ networkConnected NOTIFY refreshed)
    Q_PROPERTY(bool stickyKeys READ stickyKeys NOTIFY refreshed)
    Q_PROPERTY(bool magnifier READ magnifier NOTIFY refreshed)
    Q_PROPERTY(bool screenReaderAvailable READ screenReaderAvailable NOTIFY refreshed)
    Q_PROPERTY(bool screenReader READ screenReader NOTIFY refreshed)

public:
    explicit Security(QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    bool canSwitchUser() const { return m_canSwitchUser; }
    bool canSignOut() const { return m_canSignOut; }
    bool canChangePassword() const;
    bool canTaskManager() const;
    bool canSleep() const { return m_canSleep; }
    bool canShutDown() const { return m_canShutDown; }
    bool networkAvailable() const { return m_networkAvailable; }
    QString networkText() const { return m_networkText; }
    bool networkConnected() const { return m_networkConnected; }
    bool stickyKeys() const;
    bool magnifier() const { return m_magnifier; }
    bool screenReaderAvailable() const;
    bool screenReader() const;

    Q_INVOKABLE void show();
    Q_INVOKABLE void close();

    Q_INVOKABLE void lock();
    Q_INVOKABLE void switchUser();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void changePassword();
    Q_INVOKABLE void taskManager();
    Q_INVOKABLE void sleep();
    Q_INVOKABLE void shutDown();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void networkSettings();
    Q_INVOKABLE void setStickyKeys(bool on);
    Q_INVOKABLE void setMagnifier(bool on);
    Q_INVOKABLE void setScreenReader(bool on);

    // Set when this process is the screen itself (no session service to
    // hand it to): closing then ends the process.
    void setQuitOnClose(bool quit) { m_quitOnClose = quit; }

Q_SIGNALS:
    void openChanged();
    void refreshed();

private:
    void refresh();
    void lockSession();
    void shutdownCall(const QString &kdeMethod, const QString &logindMethod);

    bool m_open = false;
    bool m_quitOnClose = false;
    bool m_canSwitchUser = false;
    bool m_canSignOut = false;
    bool m_canSleep = false;
    bool m_canShutDown = false;
    bool m_networkAvailable = false;
    bool m_networkConnected = false;
    QString m_networkText;
    bool m_magnifier = false;
};
