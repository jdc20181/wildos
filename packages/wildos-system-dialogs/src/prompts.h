#pragma once

#include <QDBusMessage>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// The system prompts of board 8.9.
//
// In the session service (watch()):
//   battery   at the low level powerdevil uses (10% unless changed) while
//             running on battery, once per discharge, with battery saver
//             (the power-saver profile) when the computer has one
//   restart   when an installed update needs a restart (/run/reboot-
//             required, or an offline update PackageKit has prepared):
//             restart now, pick a time, or not now (asked again in four
//             hours). Nothing restarts unless the person chose it; at a
//             time they picked there is still a minute's warning they can
//             stop.
//
// One-shot, as a process of its own:
//   notresponding  in place of KWin's kill helper, with KWin's arguments
//   windowsmode    from the App Bridge when a Windows app is opened while
//                  Windows app support is turned off
//
// Asked by Windows mode over D-Bus (session.h), answered when chosen:
//   windowsshutdown  Windows apps are open as the session ends (9.9); the
//                    safe choice, keep, is taken after 60 seconds
//   windowsstarting  a Windows app is waiting for Windows to start or wake
//   windowsdevices   a call app in Windows mode wants the camera and
//                    microphone (9.10)
class Prompts : public QObject
{
    Q_OBJECT

    // battery, restart, notresponding, windowsmode or empty
    Q_PROPERTY(QString showing READ showing NOTIFY showingChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    Q_PROPERTY(QVariantList restartTimes READ restartTimes NOTIFY infoChanged)
    Q_PROPERTY(int countdown READ countdown NOTIFY countdownChanged)

public:
    explicit Prompts(QObject *parent = nullptr);

    QString showing() const { return m_showing; }
    QVariantMap info() const { return m_info; }
    QVariantList restartTimes() const { return m_times; }
    int countdown() const { return m_countdown; }

    void watch();
    void notResponding(qint64 pid, const QString &appId, const QString &caption, const QString &handle, bool local);
    void windowsMode(const QString &id, const QString &name, const QString &icon, const QString &target);
    Q_INVOKABLE void demo(const QString &kind);

    Q_INVOKABLE void showRestart();
    Q_INVOKABLE void dismiss();
    // Battery
    Q_INVOKABLE void batterySaver();
    // Restart
    Q_INVOKABLE void restartNow();
    Q_INVOKABLE void schedule(const QString &when);
    Q_INVOKABLE void notNow();
    // Not responding
    Q_INVOKABLE void endTask();
    // Windows mode
    Q_INVOKABLE void useWindowsMode();
    void askWindowsShutdown(const QString &action, const QString &appsJson, const QDBusMessage &reply);
    // keep, discard or back
    Q_INVOKABLE void answerWindowsShutdown(const QString &choice);
    void askWindowsDevices(const QString &appName, const QString &devicesJson, const QDBusMessage &reply);
    Q_INVOKABLE void answerWindowsDevices(bool allow, const QString &camera, const QString &microphone);
    // Windows is starting so an app can open; yields to any other prompt.
    void windowsStarting(const QString &appName, const QString &iconName, int seconds, bool cold);
    void windowsStartingDone();

    // Where Settings reads the restart Wild OS will make, if one is set.
    static QString configFile() { return QStringLiteral("wildos-restartrc"); }

private Q_SLOTS:
    void upowerChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

Q_SIGNALS:
    void showingChanged();
    void infoChanged();
    void countdownChanged();

private:
    void show(const QString &kind);
    void readBattery();
    void onBatteryChanged();
    void checkRestart();
    void buildTimes();
    QDateTime scheduled() const;
    void setScheduled(const QDateTime &when);
    bool restartPending() const;

    QString m_showing;
    QVariantMap m_info;
    QVariantList m_times;
    bool m_oneShot = false;
    bool m_demo = false;

    double m_percent = 100;
    uint m_state = 0;
    qint64 m_timeToEmpty = 0;
    bool m_isBattery = false;
    bool m_batteryWarned = false;

    QTimer m_restartCheck;
    QTimer m_scheduleTimer;
    QTimer m_countdownTimer;
    int m_countdown = 0;
    QDateTime m_snoozedUntil;
    bool m_restartAsked = false;

    qint64 m_pid = 0;
    QString m_windowsId;
    QString m_windowsTarget;
    QDBusMessage m_shutdownReply;
    QDBusMessage m_devicesReply;
    QTimer m_shutdownTimer;
};
