#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QThread>

// Flathub support through libflatpak. Apps go into the user's own
// installation: no password is needed, and the Flathub remote is added there
// the first time the Store runs.
//
// The work runs on three threads, each with its own FlatpakInstallation, so
// none waits on another: one keeps the catalogue and the update list fresh,
// one answers app pages (permissions and download sizes), and one installs.
// libflatpak takes the repository locks that make this safe, the same ones
// that let two flatpak commands run at once.
class FlatpakWorker;

// What installing an app would download, from a transaction resolved and
// then stopped before it fetches anything.
struct FlatpakPlan {
    qint64 total = -1;
    qint64 shared = -1;
    QString runtime;
    // Room it takes once unpacked, which is what has to be free.
    qint64 installed = -1;
};
Q_DECLARE_METATYPE(FlatpakPlan)

using FlatpakBatch = QList<QPair<int, QString>>;

// The error both backends finish a job with when it was cancelled, so the
// Store can tell a cancel from a failure.
inline constexpr char cancelledMarker[] = "cancelled";

class FlatpakBackend : public QObject
{
    Q_OBJECT

public:
    explicit FlatpakBackend(QObject *parent = nullptr);
    ~FlatpakBackend() override;

    void start();
    // Each pair is a job and the app ref it installs; they share one
    // transaction, so a runtime two of them need is fetched once.
    void install(const FlatpakBatch &jobs);
    void uninstall(int job, const QString &ref);
    void update(int job, const QStringList &refs);
    // Stops the transaction the job is in, at once, from this thread.
    void cancel(int job);
    void fetchDetails(const QString &appId, const QString &ref);
    void refreshAppstream();
    // A .flatpakref or .flatpak file someone opened. Local jobs have
    // negative numbers, so the Catalog's queue never mistakes them for its
    // own; they report through progress() and finished() like the rest.
    void installFile(int job, const QString &path, bool bundle);

    QString appstreamDir() const { return m_appstreamDir; }
    QSet<QString> installed() const { return m_installed; }
    QSet<QString> updates() const { return m_updates; }
    QHash<QString, qint64> installedSizes() const { return m_sizes; }
    // When each installed version arrived, and what each update downloads.
    QHash<QString, qint64> updatedTimes() const { return m_updated; }
    QHash<QString, qint64> updateSizes() const { return m_updateSizes; }
    bool isReady() const { return m_ready; }
    // Seeds the installed list from the last run until the real one is in.
    void seedInstalled(const QSet<QString> &apps);

    static QString userDataDir();

Q_SIGNALS:
    void appstreamChanged();
    void installedChanged();
    void updatesChanged();
    // detail names a shared runtime while it downloads, with its size.
    void progress(int job, int percent, const QString &status, const QString &detail, qint64 done, qint64 total);
    void finished(int job, bool ok, const QString &error);
    void metadataReady(const QString &appId, const QString &metadata);
    void planReady(const QString &appId, const FlatpakPlan &plan);
    void unavailable(const QString &reason);
    // A check asked for with refreshAppstream() is over.
    void refreshFinished(bool ok);

    // Internal, queued to the workers.
    void requestStart();
    void requestInstall(const FlatpakBatch &jobs);
    void requestUninstall(int job, const QString &ref);
    void requestUpdate(int job, const QStringList &refs);
    void requestDetails(const QString &appId, const QString &ref);
    void requestCheck();
    void requestUpdateCheck();
    void requestInstallFile(int job, const QString &path, bool bundle);

private:
    void checkUpdatesSoon();

    QThread m_catalogueThread;
    QThread m_detailsThread;
    QThread m_installThread;
    FlatpakWorker *m_catalogue = nullptr;
    FlatpakWorker *m_details = nullptr;
    FlatpakWorker *m_installer = nullptr;
    QString m_appstreamDir;
    QSet<QString> m_installed;
    QSet<QString> m_updates;
    QHash<QString, qint64> m_sizes;
    QHash<QString, qint64> m_updated;
    QHash<QString, qint64> m_updateSizes;
    bool m_ready = false;
    bool m_updateCheckQueued = false;
};

class FlatpakWorker : public QObject
{
    Q_OBJECT

public:
    explicit FlatpakWorker(const char *role);
    ~FlatpakWorker() override;

    // Called from the window's thread while a transaction runs on this one.
    void cancel(int job);

public Q_SLOTS:
    void start();
    void install(const FlatpakBatch &jobs);
    void uninstall(int job, const QString &ref);
    void update(int job, const QStringList &refs);
    void installFile(int job, const QString &path, bool bundle);
    void details(const QString &appId, const QString &ref);
    bool appstream(bool force);
    void check();
    void checkUpdates();

Q_SIGNALS:
    void appstreamReady(const QString &dir);
    void installedListed(const QStringList &apps, const QHash<QString, qint64> &sizes, const QHash<QString, qint64> &updated);
    void updatesListed(const QStringList &updates, const QHash<QString, qint64> &sizes);
    void progress(int job, int percent, const QString &status, const QString &detail, qint64 done, qint64 total);
    void finished(int job, bool ok, const QString &error);
    void metadataReady(const QString &appId, const QString &metadata);
    void planReady(const QString &appId, const FlatpakPlan &plan);
    void unavailable(const QString &reason);
    void checked(bool ok);

private:
    enum class Op { Install, Uninstall, Update, InstallRefFile, InstallBundle };
    bool ensureInstallation();
    void listInstalled();
    void runTransaction(const QList<int> &jobs, Op op, const QStringList &refs);
    bool plan(const QString &appId, const QString &ref);
    QHash<QString, qint64> updateSizes(const QStringList &refs);

    const char *m_role;
    struct _FlatpakInstallation *m_installation = nullptr;
    QMutex m_runningMutex;
    QList<int> m_runningJobs;
    struct _GCancellable *m_cancellable = nullptr;
};
