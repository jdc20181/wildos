#pragma once

#include "sftp.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class QFileSystemWatcher;

// Copying files between this PC and a server (RemoteFiles.dc): the two
// folders side by side, and the copies in progress below them. Folders are
// copied with everything in them.
class FileBrowser : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString localPath READ localPath NOTIFY localChanged)
    Q_PROPERTY(QString localShown READ localShown NOTIFY localChanged)
    Q_PROPERTY(QVariantList localEntries READ localEntries NOTIFY localChanged)
    Q_PROPERTY(QString remotePath READ remotePath NOTIFY remoteChanged)
    Q_PROPERTY(QVariantList remoteEntries READ remoteEntries NOTIFY remoteChanged)
    Q_PROPERTY(bool remoteReady READ remoteReady NOTIFY remoteChanged)
    Q_PROPERTY(QString remoteError READ remoteError NOTIFY remoteChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY remoteChanged)
    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)
    Q_PROPERTY(QString transferTitle READ transferTitle NOTIFY transfersChanged)
    Q_PROPERTY(QString transferSummary READ transferSummary NOTIFY transfersChanged)
    Q_PROPERTY(bool transferring READ transferring NOTIFY transfersChanged)

public:
    FileBrowser(const QString &hostName, QObject *parent = nullptr);
    ~FileBrowser() override;

    // Connects through the terminal's shared connection.
    void connectTo(const QStringList &sshArgs);
    void disconnectNow();

    QString localPath() const { return m_localPath; }
    QString localShown() const;
    QVariantList localEntries() const { return m_localEntries; }
    QString remotePath() const { return m_remotePath; }
    QVariantList remoteEntries() const { return m_remoteEntries; }
    bool remoteReady() const { return m_sftp->ready(); }
    QString remoteError() const { return m_remoteError; }
    bool loading() const { return m_loading; }
    QVariantList transfers() const;
    QString transferTitle() const;
    QString transferSummary() const;
    bool transferring() const;

    Q_INVOKABLE void openLocal(const QString &path);
    Q_INVOKABLE void openRemote(const QString &path);
    Q_INVOKABLE void localBack();
    Q_INVOKABLE void remoteBack();
    Q_INVOKABLE void refreshLocal();
    Q_INVOKABLE void refreshRemote();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void upload(const QStringList &names);
    Q_INVOKABLE void download(const QStringList &names);
    Q_INVOKABLE void newLocalFolder(const QString &name);
    Q_INVOKABLE void newRemoteFolder(const QString &name);
    Q_INVOKABLE void removeRemote(const QStringList &names);
    Q_INVOKABLE void renameRemote(const QString &from, const QString &to);
    // Opens a server file in its app here; saving it uploads it back.
    Q_INVOKABLE void editRemote(const QString &name);
    Q_INVOKABLE void openLocalFile(const QString &name);
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE void cancelTransfers();

Q_SIGNALS:
    void localChanged();
    void remoteChanged();
    void transfersChanged();
    void notice(const QString &text);

private:
    struct Job {
        bool upload = true;
        QString name;
        QString local;
        QString remote;
        qint64 size = 0;
        qint64 done = 0;
        // "waiting", "active", "done", "failed"
        QString state = QStringLiteral("waiting");
        QString error;
        // Shown in the list: a folder's files show as one row per file.
        bool silent = false;
    };

    static QVariantMap entryMap(const QString &name, bool dir, qint64 size, qint64 mtime);
    void listLocal();
    void listRemote();
    void queueUpload(const QString &local, const QString &remote, const QString &shown);
    void queueDownload(const QString &remote, const QString &local, const QString &shown, bool isDir, qint64 size);
    void startNext();
    void finishBatch();
    QString joinRemote(const QString &dir, const QString &name) const;

    QString m_hostName;
    Sftp *m_sftp;
    QStringList m_sshArgs;
    QString m_localPath;
    QStringList m_localHistory;
    QVariantList m_localEntries;
    QString m_remotePath;
    QStringList m_remoteHistory;
    QVariantList m_remoteEntries;
    QString m_remoteError;
    bool m_loading = false;

    QList<Job> m_jobs;
    int m_active = -1;
    int m_pendingLists = 0;
    QElapsedTimer m_batchClock;
    qint64 m_batchStartBytes = 0;
    QTimer m_progressTimer;
    bool m_cancelled = false;

    QFileSystemWatcher *m_watcher = nullptr;
    // A local copy being edited -> the file on the server it saves to.
    QHash<QString, QString> m_editing;
    QTimer m_saveTimer;
    QStringList m_changed;
};
