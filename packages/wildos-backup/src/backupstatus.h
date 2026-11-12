#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "sizes.h"

// What the window shows about backing up: whether a run is going (started
// here or by the timer, it's the same run, read from the status file it
// writes), its progress, the runs so far, what's waiting, Windows mode and
// the sizes of what's included. Back up now and Cancel act on that run.
class BackupStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Backup)
    QML_SINGLETON

    Q_PROPERTY(bool running READ running NOTIFY progressChanged)
    // "preparing", "files", "windows" or "tidying"
    Q_PROPERTY(QString phase READ phase NOTIFY progressChanged)
    // 0 to 1, or -1 when not known yet.
    Q_PROPERTY(double percent READ percent NOTIFY progressChanged)
    Q_PROPERTY(double filesDone READ filesDone NOTIFY progressChanged)
    Q_PROPERTY(double totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(double bytesDone READ bytesDone NOTIFY progressChanged)
    Q_PROPERTY(double totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(double secondsLeft READ secondsLeft NOTIFY progressChanged)
    Q_PROPERTY(QString runDrive READ runDrive NOTIFY progressChanged)
    Q_PROPERTY(bool stopping READ stopping NOTIFY progressChanged)

    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    // The newest run that finished, whatever its result.
    Q_PROPERTY(QVariantMap last READ last NOTIFY historyChanged)
    // The newest run that worked.
    Q_PROPERTY(QVariantMap lastDone READ lastDone NOTIFY historyChanged)
    // A scheduled backup found no drive and waits for it.
    Q_PROPERTY(QDateTime pendingSince READ pendingSince NOTIFY historyChanged)

    Q_PROPERTY(QDateTime nextRun READ nextRun NOTIFY nextRunChanged)

    Q_PROPERTY(bool windowsPresent READ windowsPresent NOTIFY windowsChanged)
    Q_PROPERTY(bool windowsSetUp READ windowsSetUp NOTIFY windowsChanged)
    Q_PROPERTY(bool windowsShutDown READ windowsShutDown NOTIFY windowsChanged)
    Q_PROPERTY(double windowsBytes READ windowsBytes NOTIFY windowsChanged)

    Q_PROPERTY(Sizes *sizes READ sizes CONSTANT)

public:
    explicit BackupStatus(QObject *parent);
    static BackupStatus *instance();
    static BackupStatus *create(QQmlEngine *, QJSEngine *);

    bool running() const
    {
        return m_running;
    }
    QString phase() const;
    double percent() const;
    double filesDone() const;
    double totalFiles() const;
    double bytesDone() const;
    double totalBytes() const;
    double secondsLeft() const;
    QString runDrive() const;
    bool stopping() const
    {
        return m_stopping;
    }

    QVariantList history() const
    {
        return m_history;
    }
    QVariantMap last() const;
    QVariantMap lastDone() const;
    QDateTime pendingSince() const
    {
        return m_pending;
    }
    QDateTime nextRun() const;

    bool windowsPresent() const
    {
        return m_windowsPresent;
    }
    bool windowsSetUp() const
    {
        return m_windowsSetUp;
    }
    bool windowsShutDown() const
    {
        return m_windowsShutDown;
    }
    double windowsBytes() const
    {
        return m_windowsBytes;
    }
    Sizes *sizes() const
    {
        return m_sizes;
    }

    // Starts a run unless one is going; it carries on if the window closes.
    Q_INVOKABLE void backUpNow();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void refreshWindows();

Q_SIGNALS:
    void progressChanged();
    void historyChanged();
    void nextRunChanged();
    void windowsChanged();
    // A run ended while the window was open.
    void runEnded(const QVariantMap &entry);

private:
    void poll();
    void loadHistory();

    QTimer m_poll;
    QTimer m_minute;
    QVariantMap m_status;
    QVariantList m_history;
    QDateTime m_pending;
    QDateTime m_historyStamp;
    QDateTime m_statusStamp;
    qint64 m_pid = 0;
    bool m_running = false;
    bool m_starting = false;
    bool m_stopping = false;
    bool m_windowsPresent = false;
    bool m_windowsSetUp = false;
    bool m_windowsShutDown = true;
    double m_windowsBytes = -1;
    Sizes *m_sizes;
};
