#include "flatpakbackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <flatpak.h>

namespace
{
const char *const remoteName = "flathub";

QString takeError(GError *&error, const QString &fallback = {})
{
    QString text = fallback;
    if (error) {
        if (text.isEmpty()) {
            text = QString::fromUtf8(error->message);
        }
        g_clear_error(&error);
    }
    return text;
}

struct TransactionState {
    FlatpakWorker *worker;
    int job;
    int opIndex = -1;
    int opCount = 1;
    bool removing = false;
    QString error;
};

void onProgressChanged(FlatpakTransactionProgress *progress, gpointer data)
{
    auto state = static_cast<TransactionState *>(data);
    const int part = qBound(0, flatpak_transaction_progress_get_progress(progress), 100);
    const int whole = (qMax(0, state->opIndex) * 100 + part) / qMax(1, state->opCount);
    const bool estimating = flatpak_transaction_progress_get_is_estimating(progress);
    QString status;
    if (state->removing) {
        status = QStringLiteral("remove");
    } else {
        status = estimating ? QStringLiteral("prepare") : QStringLiteral("download");
    }
    QMetaObject::invokeMethod(state->worker, "progress", Qt::DirectConnection, Q_ARG(int, state->job), Q_ARG(int, whole), Q_ARG(QString, status));
}

void onNewOperation(FlatpakTransaction *, FlatpakTransactionOperation *operation, FlatpakTransactionProgress *progress, gpointer data)
{
    auto state = static_cast<TransactionState *>(data);
    ++state->opIndex;
    state->removing = flatpak_transaction_operation_get_operation_type(operation) == FLATPAK_TRANSACTION_OPERATION_UNINSTALL;
    flatpak_transaction_progress_set_update_frequency(progress, 250);
    g_signal_connect(progress, "changed", G_CALLBACK(onProgressChanged), data);
    onProgressChanged(progress, data);
}

gboolean onReady(FlatpakTransaction *transaction, gpointer data)
{
    auto state = static_cast<TransactionState *>(data);
    GList *ops = flatpak_transaction_get_operations(transaction);
    state->opCount = qMax(1, int(g_list_length(ops)));
    g_list_free_full(ops, g_object_unref);
    return TRUE;
}

gboolean onOperationError(FlatpakTransaction *, FlatpakTransactionOperation *, GError *error, gint details, gpointer data)
{
    auto state = static_cast<TransactionState *>(data);
    // A related ref that fails (a translation, say) should not undo the app.
    if (details & FLATPAK_TRANSACTION_ERROR_DETAILS_NON_FATAL) {
        return TRUE;
    }
    if (error && state->error.isEmpty()) {
        state->error = QString::fromUtf8(error->message);
    }
    return FALSE;
}

gboolean onAddNewRemote(FlatpakTransaction *, FlatpakTransactionRemoteReason, const char *, const char *, const char *, gpointer)
{
    // Apps only ever come from Flathub; never add a remote an app asks for.
    return FALSE;
}

int onChooseRemote(FlatpakTransaction *, const char *, const char *, const char *const *remotes, gpointer)
{
    for (int i = 0; remotes && remotes[i]; ++i) {
        if (qstrcmp(remotes[i], remoteName) == 0) {
            return i;
        }
    }
    return 0;
}
}

FlatpakBackend::FlatpakBackend(QObject *parent)
    : QObject(parent)
{
    m_worker = new FlatpakWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &FlatpakBackend::requestStart, m_worker, &FlatpakWorker::start);
    connect(this, &FlatpakBackend::requestInstall, m_worker, &FlatpakWorker::install);
    connect(this, &FlatpakBackend::requestUninstall, m_worker, &FlatpakWorker::uninstall);
    connect(this, &FlatpakBackend::requestUpdate, m_worker, &FlatpakWorker::update);
    connect(this, &FlatpakBackend::requestMetadata, m_worker, &FlatpakWorker::metadata);
    connect(this, &FlatpakBackend::requestAppstream, m_worker, &FlatpakWorker::appstream);
    connect(this, &FlatpakBackend::requestCheck, m_worker, &FlatpakWorker::check);

    connect(m_worker, &FlatpakWorker::appstreamReady, this, [this](const QString &dir) {
        m_appstreamDir = dir;
        Q_EMIT appstreamChanged();
    });
    connect(m_worker, &FlatpakWorker::installedListed, this,
            [this](const QStringList &apps, const QStringList &updates, const QHash<QString, qint64> &sizes) {
                m_installed = QSet<QString>(apps.begin(), apps.end());
                m_updates = QSet<QString>(updates.begin(), updates.end());
                m_sizes = sizes;
                m_ready = true;
                Q_EMIT installedChanged();
            });
    connect(m_worker, &FlatpakWorker::progress, this, &FlatpakBackend::progress);
    connect(m_worker, &FlatpakWorker::finished, this, &FlatpakBackend::finished);
    connect(m_worker, &FlatpakWorker::metadataReady, this, &FlatpakBackend::metadataReady);
    connect(m_worker, &FlatpakWorker::unavailable, this, &FlatpakBackend::unavailable);
    connect(m_worker, &FlatpakWorker::checked, this, &FlatpakBackend::refreshFinished);

    m_thread.setObjectName(QStringLiteral("flatpak"));
    m_thread.start();
}

FlatpakBackend::~FlatpakBackend()
{
    m_thread.quit();
    m_thread.wait(3000);
}

QString FlatpakBackend::userDataDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("flatpak"));
}

void FlatpakBackend::start()
{
    Q_EMIT requestStart();
}

void FlatpakBackend::install(int job, const QString &ref)
{
    Q_EMIT requestInstall(job, ref);
}

void FlatpakBackend::uninstall(int job, const QString &ref)
{
    Q_EMIT requestUninstall(job, ref);
}

void FlatpakBackend::update(int job, const QStringList &refs)
{
    Q_EMIT requestUpdate(job, refs);
}

void FlatpakBackend::fetchMetadata(const QString &appId, const QString &ref)
{
    Q_EMIT requestMetadata(appId, ref);
}

void FlatpakBackend::refreshAppstream()
{
    Q_EMIT requestCheck();
}

FlatpakWorker::FlatpakWorker() = default;

FlatpakWorker::~FlatpakWorker()
{
    g_clear_object(&m_installation);
}

bool FlatpakWorker::ensureInstallation()
{
    if (m_installation) {
        return true;
    }
    GError *error = nullptr;
    m_installation = flatpak_installation_new_user(nullptr, &error);
    if (!m_installation) {
        Q_EMIT unavailable(takeError(error));
        return false;
    }

    FlatpakRemote *remote = flatpak_installation_get_remote_by_name(m_installation, remoteName, nullptr, &error);
    if (remote) {
        // Someone may have switched it off with the command line; the Store
        // cannot work without it.
        if (flatpak_remote_get_disabled(remote)) {
            flatpak_remote_set_disabled(remote, FALSE);
            flatpak_installation_modify_remote(m_installation, remote, nullptr, nullptr);
        }
        g_object_unref(remote);
        return true;
    }
    g_clear_error(&error);

    QFile file(QStringLiteral(":/qt/qml/WildOS/Store/data/flathub.flatpakrepo"));
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT unavailable(QStringLiteral("The Flathub details are missing from the Store."));
        return false;
    }
    const QByteArray data = file.readAll();
    GBytes *bytes = g_bytes_new(data.constData(), data.size());
    remote = flatpak_remote_new_from_file(remoteName, bytes, &error);
    g_bytes_unref(bytes);
    if (!remote) {
        Q_EMIT unavailable(takeError(error));
        return false;
    }
    flatpak_remote_set_gpg_verify(remote, TRUE);
    const bool added = flatpak_installation_add_remote(m_installation, remote, TRUE, nullptr, &error);
    g_object_unref(remote);
    if (!added) {
        Q_EMIT unavailable(takeError(error));
        return false;
    }
    flatpak_installation_drop_caches(m_installation, nullptr, nullptr);
    return true;
}

void FlatpakWorker::start()
{
    if (!ensureInstallation()) {
        return;
    }
    listInstalled();
    appstream(false);
}

bool FlatpakWorker::appstream(bool force)
{
    if (!ensureInstallation()) {
        return false;
    }
    GError *error = nullptr;
    FlatpakRemote *remote = flatpak_installation_get_remote_by_name(m_installation, remoteName, nullptr, &error);
    if (!remote) {
        takeError(error);
        return false;
    }
    GFile *dirFile = flatpak_remote_get_appstream_dir(remote, nullptr);
    GFile *stampFile = flatpak_remote_get_appstream_timestamp(remote, nullptr);
    char *dirPath = g_file_get_path(dirFile);
    char *stampPath = g_file_get_path(stampFile);
    const QString dir = QString::fromUtf8(dirPath);
    const QString stamp = QString::fromUtf8(stampPath);
    g_free(dirPath);
    g_free(stampPath);
    g_object_unref(dirFile);
    g_object_unref(stampFile);
    g_object_unref(remote);

    const bool present = QFileInfo::exists(dir + QStringLiteral("/appstream.xml.gz")) || QFileInfo::exists(dir + QStringLiteral("/appstream.xml"));
    if (present) {
        Q_EMIT appstreamReady(dir);
    }

    // Flathub's catalogue changes many times a day; once a day is plenty for
    // browsing, and installs always fetch the current version anyway.
    const QFileInfo stampInfo(stamp);
    const bool stale = !stampInfo.exists() || stampInfo.lastModified().secsTo(QDateTime::currentDateTime()) > 24 * 3600;
    if (!force && present && !stale) {
        return true;
    }
    gboolean changed = FALSE;
    if (!flatpak_installation_update_appstream_full_sync(m_installation, remoteName, nullptr, nullptr, nullptr, &changed, nullptr, &error)) {
        const QString message = takeError(error);
        if (!present) {
            Q_EMIT unavailable(message);
        }
        return false;
    }
    if (changed || !present) {
        Q_EMIT appstreamReady(dir);
    }
    // Updates are only known once the remote's summary is fresh.
    listInstalled();
    return true;
}

void FlatpakWorker::check()
{
    Q_EMIT checked(appstream(true));
}

void FlatpakWorker::listInstalled()
{
    GError *error = nullptr;
    QStringList apps;
    QStringList updates;
    QHash<QString, qint64> sizes;

    GPtrArray *refs = flatpak_installation_list_installed_refs_by_kind(m_installation, FLATPAK_REF_KIND_APP, nullptr, &error);
    if (refs) {
        for (guint i = 0; i < refs->len; ++i) {
            auto ref = static_cast<FlatpakInstalledRef *>(g_ptr_array_index(refs, i));
            const QString name = QString::fromUtf8(flatpak_ref_get_name(FLATPAK_REF(ref)));
            apps << name;
            sizes.insert(name, qint64(flatpak_installed_ref_get_installed_size(ref)));
        }
        g_ptr_array_unref(refs);
    }
    takeError(error);

    refs = flatpak_installation_list_installed_refs_for_update(m_installation, nullptr, &error);
    if (refs) {
        for (guint i = 0; i < refs->len; ++i) {
            auto ref = static_cast<FlatpakInstalledRef *>(g_ptr_array_index(refs, i));
            if (flatpak_ref_get_kind(FLATPAK_REF(ref)) == FLATPAK_REF_KIND_APP) {
                updates << QString::fromUtf8(flatpak_ref_get_name(FLATPAK_REF(ref)));
            }
        }
        g_ptr_array_unref(refs);
    }
    takeError(error);

    Q_EMIT installedListed(apps, updates, sizes);
}

void FlatpakWorker::install(int job, const QString &ref)
{
    runTransaction(job, Op::Install, {ref});
}

void FlatpakWorker::uninstall(int job, const QString &ref)
{
    runTransaction(job, Op::Uninstall, {ref});
}

void FlatpakWorker::update(int job, const QStringList &refs)
{
    runTransaction(job, Op::Update, refs);
}

void FlatpakWorker::runTransaction(int job, Op op, const QStringList &refs)
{
    if (!ensureInstallation()) {
        Q_EMIT finished(job, false, QStringLiteral("Flathub is not available right now."));
        return;
    }

    // The transaction iterates the thread's default context while it
    // downloads, which is what delivers the progress signals here.
    GMainContext *context = g_main_context_new();
    g_main_context_push_thread_default(context);

    GError *error = nullptr;
    FlatpakTransaction *transaction = flatpak_transaction_new_for_installation(m_installation, nullptr, &error);
    QString failure;
    if (!transaction) {
        failure = takeError(error);
    } else {
        flatpak_transaction_set_no_interaction(transaction, TRUE);
        flatpak_transaction_add_default_dependency_sources(transaction);
        if (op == Op::Uninstall) {
            // Runtimes nothing else uses go with the app, as Windows would.
            flatpak_transaction_set_include_unused_uninstall_ops(transaction, TRUE);
        }

        QStringList targets = refs;
        if (op == Op::Update && targets.isEmpty()) {
            GPtrArray *pending = flatpak_installation_list_installed_refs_for_update(m_installation, nullptr, &error);
            if (pending) {
                for (guint i = 0; i < pending->len; ++i) {
                    char *full = flatpak_ref_format_ref(FLATPAK_REF(g_ptr_array_index(pending, i)));
                    targets << QString::fromUtf8(full);
                    g_free(full);
                }
                g_ptr_array_unref(pending);
            }
            takeError(error);
        }

        for (const QString &ref : std::as_const(targets)) {
            const QByteArray r = ref.toUtf8();
            bool added = false;
            switch (op) {
            case Op::Install:
                added = flatpak_transaction_add_install(transaction, remoteName, r.constData(), nullptr, &error);
                break;
            case Op::Uninstall:
                added = flatpak_transaction_add_uninstall(transaction, r.constData(), &error);
                break;
            case Op::Update:
                added = flatpak_transaction_add_update(transaction, r.constData(), nullptr, nullptr, &error);
                break;
            }
            if (!added) {
                if (error && g_error_matches(error, FLATPAK_ERROR, FLATPAK_ERROR_ALREADY_INSTALLED)) {
                    g_clear_error(&error);
                    continue;
                }
                failure = takeError(error);
                break;
            }
        }

        if (failure.isEmpty() && !flatpak_transaction_is_empty(transaction)) {
            TransactionState state{this, job};
            g_signal_connect(transaction, "new-operation", G_CALLBACK(onNewOperation), &state);
            g_signal_connect(transaction, "ready", G_CALLBACK(onReady), &state);
            g_signal_connect(transaction, "operation-error", G_CALLBACK(onOperationError), &state);
            g_signal_connect(transaction, "add-new-remote", G_CALLBACK(onAddNewRemote), &state);
            g_signal_connect(transaction, "choose-remote-for-ref", G_CALLBACK(onChooseRemote), &state);
            if (!flatpak_transaction_run(transaction, nullptr, &error)) {
                failure = state.error.isEmpty() ? takeError(error) : state.error;
                g_clear_error(&error);
                if (failure.isEmpty()) {
                    failure = QStringLiteral("Flathub stopped part way through.");
                }
            }
        }
        g_object_unref(transaction);
    }

    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);

    flatpak_installation_drop_caches(m_installation, nullptr, nullptr);
    listInstalled();
    Q_EMIT finished(job, failure.isEmpty(), failure);
}

void FlatpakWorker::metadata(const QString &appId, const QString &ref)
{
    if (!ensureInstallation()) {
        return;
    }
    GError *error = nullptr;
    GBytes *bytes = nullptr;
    FlatpakInstalledRef *installed = flatpak_installation_get_installed_ref(m_installation, FLATPAK_REF_KIND_APP, appId.toUtf8().constData(),
                                                                            nullptr, nullptr, nullptr, &error);
    if (installed) {
        bytes = flatpak_installed_ref_load_metadata(installed, nullptr, &error);
        g_object_unref(installed);
    }
    g_clear_error(&error);
    if (!bytes && !ref.isEmpty()) {
        FlatpakRef *parsed = flatpak_ref_parse(ref.toUtf8().constData(), &error);
        if (parsed) {
            bytes = flatpak_installation_fetch_remote_metadata_sync(m_installation, remoteName, parsed, nullptr, &error);
            g_object_unref(parsed);
        }
        g_clear_error(&error);
    }
    if (!bytes) {
        // Said out loud, so the app's page stops waiting for it.
        Q_EMIT metadataReady(appId, QString());
        return;
    }
    gsize size = 0;
    const auto data = static_cast<const char *>(g_bytes_get_data(bytes, &size));
    const QString text = QString::fromUtf8(data, qsizetype(size));
    g_bytes_unref(bytes);
    Q_EMIT metadataReady(appId, text);
}
