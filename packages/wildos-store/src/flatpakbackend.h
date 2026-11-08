#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThread>

// Flathub support through libflatpak. Everything that touches the network or
// the disk runs on a worker thread, so the window never stalls on a download.
// Apps go into the user's own installation: no password is needed, and the
// Flathub remote is added there the first time the Store runs.
class FlatpakWorker;

class FlatpakBackend : public QObject
{
    Q_OBJECT

public:
    explicit FlatpakBackend(QObject *parent = nullptr);
    ~FlatpakBackend() override;

    void start();
    void install(int job, const QString &ref);
    void uninstall(int job, const QString &ref);
    void update(int job, const QStringList &refs);
    void fetchMetadata(const QString &appId, const QString &ref);
    void refreshAppstream();

    QString appstreamDir() const { return m_appstreamDir; }
    QSet<QString> installed() const { return m_installed; }
    QSet<QString> updates() const { return m_updates; }
    QHash<QString, qint64> installedSizes() const { return m_sizes; }
    bool isReady() const { return m_ready; }

    static QString userDataDir();

Q_SIGNALS:
    void appstreamChanged();
    void installedChanged();
    void progress(int job, int percent, const QString &status);
    void finished(int job, bool ok, const QString &error);
    void metadataReady(const QString &appId, const QString &metadata);
    void unavailable(const QString &reason);
    // A check asked for with refreshAppstream() is over.
    void refreshFinished(bool ok);

    // Internal, queued to the worker.
    void requestStart();
    void requestInstall(int job, const QString &ref);
    void requestUninstall(int job, const QString &ref);
    void requestUpdate(int job, const QStringList &refs);
    void requestMetadata(const QString &appId, const QString &ref);
    void requestAppstream(bool force);
    void requestCheck();

private:
    QThread m_thread;
    FlatpakWorker *m_worker = nullptr;
    QString m_appstreamDir;
    QSet<QString> m_installed;
    QSet<QString> m_updates;
    QHash<QString, qint64> m_sizes;
    bool m_ready = false;
};

class FlatpakWorker : public QObject
{
    Q_OBJECT

public:
    FlatpakWorker();
    ~FlatpakWorker() override;

public Q_SLOTS:
    void start();
    void install(int job, const QString &ref);
    void uninstall(int job, const QString &ref);
    void update(int job, const QStringList &refs);
    void metadata(const QString &appId, const QString &ref);
    bool appstream(bool force);
    void check();

Q_SIGNALS:
    void appstreamReady(const QString &dir);
    void installedListed(const QStringList &apps, const QStringList &updates, const QHash<QString, qint64> &sizes);
    void progress(int job, int percent, const QString &status);
    void finished(int job, bool ok, const QString &error);
    void metadataReady(const QString &appId, const QString &metadata);
    void unavailable(const QString &reason);
    void checked(bool ok);

private:
    enum class Op { Install, Uninstall, Update };
    bool ensureInstallation();
    void listInstalled();
    void runTransaction(int job, Op op, const QStringList &refs);

    struct _FlatpakInstallation *m_installation = nullptr;
};
