#pragma once

#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "store.h"

class Activation;
class ActivationFeedback;

// wildos-launchd: follows each app launch until the app's window appears.
//
// KWin announces every launch through its activation feedback, but ends it
// as soon as any other window is activated, while the app may still be on
// its way. The daemon keeps following the launch until a window of the app
// opens (the KWin script org.wildos.launch reports windows here over
// D-Bus), its process ends without one, or it gives up, and says so on
// D-Bus so the taskbar's starting line lasts exactly as long as the start.
//
// Along the way it counts launches and, once an app has settled, notes the
// files its process read; the --preload pass reads the most used apps'
// files into memory after login so they start quickly.
class Launcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Launch")

public:
    explicit Launcher(QObject *parent = nullptr);
    ~Launcher() override;

public Q_SLOTS:
    // From the KWin script, as one JSON string, because a script's D-Bus
    // arguments arrive as loosely typed values.
    //
    // {"open": [{"k": [keys], "pid": n}, ...], "added": {"k": [keys], "pid": n} | null}
    Q_SCRIPTABLE void Windows(const QString &json);
    // What is being followed, as JSON, for the taskbar after a restart and
    // for checking from a terminal.
    Q_SCRIPTABLE QString State() const;

Q_SIGNALS:
    // Every launch KWin announces.
    Q_SCRIPTABLE void LaunchStarted(const QString &desktopId);
    // A launch the daemon follows until LaunchEnded: the app had no window
    // open yet.
    Q_SCRIPTABLE void LaunchWaiting(const QString &desktopId);
    Q_SCRIPTABLE void LaunchEnded(const QString &desktopId);

private Q_SLOTS:
    void unitNew(const QString &name, const QDBusObjectPath &path);
    void unitRemoved(const QString &name, const QDBusObjectPath &path);

private:
    struct Launch {
        QString id;
        QString desktopId;
        QStringList keys;
        // Runs in Windows mode (X-WildOS-Engine=windows-mode), which can
        // take a while when Windows has to start first.
        bool windowsMode = false;
        QString unit;
        QElapsedTimer clock;
        QTimer *giveUpTimer = nullptr;
    };

    void activationStarted(Activation *activation, const QString &appId);
    void finish(const QString &id);
    void sampleLater(const QString &desktopId, int pid);
    bool anyOpen(const QStringList &keys) const;

    ActivationFeedback *m_feedback = nullptr;
    UsageStore m_usage;

    QHash<QString, Launch> m_launches;
    QHash<QString, int> m_open;
    QSet<QString> m_sampled;
    int m_next = 0;
};
