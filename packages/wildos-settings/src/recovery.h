#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

class QProcess;

// Settings > System > Recovery: restore points, going back, start-up
// options and Reset Wild OS. Reads the index the restore-points tool keeps
// (wildos-recovery) and asks for changes through one root helper,
// settings-recovery, behind the admin password.
class Recovery : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // wildos-recovery is installed and the disk can have restore points.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // "btrfs" (snapshots) or "ext4" (file copies).
    Q_PROPERTY(QString backend READ backend NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(int keep READ keep NOTIFY changed)
    Q_PROPERTY(int maxGb READ maxGb NOTIFY changed)
    Q_PROPERTY(double usedBytes READ usedBytes NOTIFY changed)
    // Newest first: {id, label, when (date), summary, apps, system, driver,
    // build, kind, holdable}.
    Q_PROPERTY(QVariantList points READ points NOTIFY changed)
    // A go-back is staged (ext4) or done (Btrfs) and waits for a restart.
    Q_PROPERTY(bool restartPending READ restartPending NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY busyChanged)
    // Start-up recovery's files are on /boot (the reset copy is verified).
    Q_PROPERTY(bool startupRecovery READ startupRecovery NOTIFY changed)
    // The reset copy: ready, or why not ("missing", "unsigned", "damaged").
    Q_PROPERTY(bool resetReady READ resetReady NOTIFY changed)
    Q_PROPERTY(QString resetWhy READ resetWhy NOTIFY changed)
    // Remove everything runs the copy of Wild OS from memory while the disk
    // is wiped, which needs about 4 GB.
    Q_PROPERTY(bool wipeFits READ wipeFits CONSTANT)
    // The Backup app, when installed (wildos-backup).
    Q_PROPERTY(bool backupInstalled READ backupInstalled NOTIFY changed)
    // When Backup last finished on any drive (invalid: never, or no Backup).
    Q_PROPERTY(QDateTime lastBackup READ lastBackup NOTIFY changed)

public:
    explicit Recovery(QObject *parent = nullptr);

    bool available() const { return m_available; }
    QString backend() const { return m_backend; }
    bool enabled() const { return m_enabled; }
    int keep() const { return m_keep; }
    int maxGb() const { return m_maxGb; }
    double usedBytes() const { return m_usedBytes; }
    QVariantList points() const { return m_points; }
    bool restartPending() const { return m_restartPending; }
    bool busy() const { return m_busy; }
    QString busyText() const { return m_busyText; }
    QString error() const { return m_error; }
    bool startupRecovery() const { return m_startupRecovery; }
    bool resetReady() const { return m_resetReady; }
    QString resetWhy() const { return m_resetWhy; }
    bool wipeFits() const;
    bool backupInstalled() const;
    QDateTime lastBackup() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void create();
    Q_INVOKABLE void goBack(int id, bool hold);
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE void setLimits(int keep, int gb, bool on);
    // Restart into start-up recovery once (Start-up options > Restart now).
    Q_INVOKABLE void restartToRecovery();
    // "keep" or "everything": staged, then the PC restarts into recovery.
    Q_INVOKABLE void reset(const QString &how);
    Q_INVOKABLE void openBackup();
    Q_INVOKABLE void restart();
    // The restore point taken just before Wild OS build `build` installed,
    // or -1: the newest point named after that build.
    Q_INVOKABLE int pointForBuild(int build) const;
    Q_INVOKABLE QVariantMap point(int id) const;

Q_SIGNALS:
    void changed();
    void busyChanged();
    // A go-back or reset is ready; the page offers the restart.
    void finished(const QString &what);

private:
    void run(const QStringList &args, const QString &text, const QString &what);
    void setBusy(bool busy, const QString &text = {});
    QString indexPath() const;

    QFileSystemWatcher m_watcher;
    QProcess *m_process = nullptr;
    bool m_available = false;
    QString m_backend;
    bool m_enabled = true;
    int m_keep = 5;
    int m_maxGb = 20;
    double m_usedBytes = 0;
    QVariantList m_points;
    bool m_restartPending = false;
    bool m_busy = false;
    QString m_busyText;
    QString m_error;
    bool m_startupRecovery = false;
    bool m_resetReady = false;
    QString m_resetWhy;
};
