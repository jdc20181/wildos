#include "deskops.h"

#include "format.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KCoreDirLister>
#include <KDesktopFile>
#include <KDirWatch>
#include <KFileUtils>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/EmptyTrashJob>
#include <KIO/JobTracker>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/Paste>
#include <KIO/StoredTransferJob>
#include <KJobTrackerInterface>
#include <KJobUiDelegate>
#include <KService>
#include <KSharedConfig>
#include <KTerminalLauncherJob>
#include <KUrlMimeData>
#include <KWaylandExtras>
#include <KWindowSystem>
#include <KX11Extras>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPointer>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QWindow>

namespace
{
QString folderName(const QUrl &url)
{
    if (url.isLocalFile()) {
        const QString path = QDir::cleanPath(url.toLocalFile());
        if (path == QDir::homePath()) {
            return QStringLiteral("Home");
        }
        if (path == QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))) {
            return QStringLiteral("The desktop");
        }
    }
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? QStringLiteral("This folder") : name;
}

QVariantMap appEntry(const KService::Ptr &service)
{
    return {
        {QStringLiteral("id"), service->storageId()},
        {QStringLiteral("name"), service->name()},
        {QStringLiteral("icon"), service->icon()},
    };
}

QString filesConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/wildosfilesrc");
}
}

DeskAsker::DeskAsker(QObject *parent)
    : KIO::AskUserActionInterface(parent)
{
}

void DeskAsker::askUserRename(KJob *job, const QString &, const QUrl &, const QUrl &dest, KIO::RenameDialog_Options options,
                              KIO::filesize_t sizeSrc, KIO::filesize_t sizeDest, const QDateTime &, const QDateTime &,
                              const QDateTime &mtimeSrc, const QDateTime &mtimeDest)
{
    const bool multiple = options & KIO::RenameDialog_MultipleItems;
    const bool folders = (options & KIO::RenameDialog_SourceIsDirectory) && (options & KIO::RenameDialog_DestIsDirectory);
    const bool itself = options & KIO::RenameDialog_OverwriteItself;
    const QString name = Format::visibleName(dest.fileName(), folders, DeskOps::instance()->showExtensions());
    const QString where = folderName(KIO::upUrl(dest));

    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("conflict");
    prompt[QStringLiteral("multiple")] = multiple;
    if (folders) {
        prompt[QStringLiteral("title")] = QStringLiteral("Merge folders?");
        prompt[QStringLiteral("text")] = QStringLiteral("%1 already has a folder called \"%2\". Merging puts what you are copying inside it; files with the same name will be asked about one by one.").arg(where, name);
        prompt[QStringLiteral("replace")] = QStringLiteral("Merge folders");
    } else if (itself) {
        prompt[QStringLiteral("title")] = QStringLiteral("This is the same file");
        prompt[QStringLiteral("text")] = QStringLiteral("\"%1\" is already here. Keep both to make a copy next to it.").arg(name);
    } else {
        prompt[QStringLiteral("title")] = QStringLiteral("Replace or skip files");
        prompt[QStringLiteral("text")] = QStringLiteral("%1 already has a file called \"%2\".").arg(where, name);
        prompt[QStringLiteral("replace")] = QStringLiteral("Replace the file");
        prompt[QStringLiteral("incoming")] = Format::size(qint64(sizeSrc)) + QStringLiteral(", ") + Format::date(mtimeSrc);
        prompt[QStringLiteral("existing")] = Format::size(qint64(sizeDest)) + QStringLiteral(", ") + Format::date(mtimeDest);
    }
    prompt[QStringLiteral("canReplace")] = !itself && (options & KIO::RenameDialog_Overwrite);
    prompt[QStringLiteral("canSkip")] = bool(options & KIO::RenameDialog_Skip) || multiple;
    prompt[QStringLiteral("canKeepBoth")] = !(options & KIO::RenameDialog_NoRename);

    QPointer<KJob> guard(job);
    DeskOps::instance()->ask(prompt, [this, guard, dest](const QString &choice, bool all) {
        if (!guard) {
            return;
        }
        if (choice == QStringLiteral("replace")) {
            Q_EMIT askUserRenameResult(all ? KIO::Result_OverwriteAll : KIO::Result_Overwrite, QUrl(), guard);
        } else if (choice == QStringLiteral("skip")) {
            Q_EMIT askUserRenameResult(all ? KIO::Result_AutoSkip : KIO::Result_Skip, QUrl(), guard);
        } else if (choice == QStringLiteral("both")) {
            if (all) {
                Q_EMIT askUserRenameResult(KIO::Result_AutoRename, QUrl(), guard);
            } else {
                const QUrl folder = KIO::upUrl(dest);
                QUrl renamed = folder;
                renamed.setPath(folder.path() + (folder.path().endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/"))
                                + KFileUtils::suggestName(folder, dest.fileName()));
                Q_EMIT askUserRenameResult(KIO::Result_Rename, renamed, guard);
            }
        } else {
            Q_EMIT askUserRenameResult(KIO::Result_Cancel, QUrl(), guard);
        }
    });
}

void DeskAsker::askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText)
{
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("skip");
    prompt[QStringLiteral("title")] = QStringLiteral("A file could not be copied");
    prompt[QStringLiteral("text")] = errorText;
    prompt[QStringLiteral("multiple")] = bool(options & KIO::SkipDialog_MultipleItems);
    QPointer<KJob> guard(job);
    DeskOps::instance()->ask(prompt, [this, guard](const QString &choice, bool all) {
        if (!guard) {
            return;
        }
        if (choice == QStringLiteral("skip")) {
            Q_EMIT askUserSkipResult(all ? KIO::Result_AutoSkip : KIO::Result_Skip, guard);
        } else if (choice == QStringLiteral("retry")) {
            Q_EMIT askUserSkipResult(KIO::Result_Retry, guard);
        } else {
            Q_EMIT askUserSkipResult(KIO::Result_Cancel, guard);
        }
    });
}

void DeskAsker::askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType, QWidget *parent)
{
    if (deletionType == Trash) {
        Q_EMIT askUserDeleteResult(true, urls, deletionType, parent);
        return;
    }
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = deletionType == EmptyTrash ? QStringLiteral("Empty the Recycle Bin?") : QStringLiteral("Delete for good?");
    prompt[QStringLiteral("text")] = QStringLiteral("You cannot undo this.");
    prompt[QStringLiteral("primary")] = deletionType == EmptyTrash ? QStringLiteral("Empty Recycle Bin") : QStringLiteral("Delete for good");
    DeskOps::instance()->ask(prompt, [this, urls, deletionType, parent](const QString &choice, bool) {
        Q_EMIT askUserDeleteResult(choice == QStringLiteral("primary"), urls, deletionType, parent);
    });
}

void DeskAsker::requestUserMessageBox(MessageDialogType type, const QString &text, const QString &title, const QString &primaryActionText,
                                      const QString &secondaryActionText, const QString &, const QString &, const QString &,
                                      const QString &, QWidget *)
{
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = title;
    prompt[QStringLiteral("text")] = text;
    prompt[QStringLiteral("primary")] = primaryActionText.isEmpty() ? QStringLiteral("Continue") : primaryActionText;
    if (type == QuestionTwoActions || type == WarningTwoActions || !secondaryActionText.isEmpty()) {
        prompt[QStringLiteral("secondary")] = secondaryActionText;
    }
    prompt[QStringLiteral("informational")] = type == Information || type == Error;
    DeskOps::instance()->ask(prompt, [this](const QString &choice, bool) {
        // KMessageDialog: PrimaryAction 3, SecondaryAction 4, Cancel 2.
        Q_EMIT messageBoxResult(choice == QStringLiteral("primary") ? 3 : choice == QStringLiteral("secondary") ? 4 : 2);
    });
}

void DeskAsker::askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *)
{
    const QString host = sslErrorData.value(QStringLiteral("hostname")).toString();
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = QStringLiteral("This connection may not be safe");
    prompt[QStringLiteral("text")] = (host.isEmpty() ? QStringLiteral("The server's") : host + QStringLiteral("'s"))
        + QStringLiteral(" security certificate cannot be checked, so someone could be listening in. Only carry on if you trust this place.");
    prompt[QStringLiteral("primary")] = QStringLiteral("Connect anyway");
    DeskOps::instance()->ask(prompt, [this](const QString &choice, bool) {
        Q_EMIT askIgnoreSslErrorsResult(choice == QStringLiteral("primary") ? 1 : 0);
    });
}

DeskOpenWithAsker::DeskOpenWithAsker(QObject *parent)
    : KIO::OpenWithHandlerInterface(parent)
{
}

void DeskOpenWithAsker::promptUserForApplication(KJob *, const QList<QUrl> &urls, const QString &)
{
    Q_EMIT canceled();
    Q_EMIT DeskOps::instance()->chooseAppRequested(urls);
}

DeskUntrustedAsker::DeskUntrustedAsker(QObject *parent)
    : KIO::UntrustedProgramHandlerInterface(parent)
{
}

void DeskUntrustedAsker::showUntrustedProgramWarning(KJob *, const QString &programName)
{
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = QStringLiteral("Run %1?").arg(programName);
    prompt[QStringLiteral("text")] = QStringLiteral("This shortcut starts a program and did not come with Wild OS or the Store. Only run it if you know where it came from.");
    prompt[QStringLiteral("primary")] = QStringLiteral("Run");
    DeskOps::instance()->ask(prompt, [this](const QString &choice, bool) {
        Q_EMIT result(choice == QStringLiteral("primary"));
    });
}

DeskOps *DeskOps::instance()
{
    static DeskOps *ops = new DeskOps;
    return ops;
}

DeskOps *DeskOps::create(QQmlEngine *, QJSEngine *)
{
    auto ops = instance();
    QJSEngine::setObjectOwnership(ops, QJSEngine::CppOwnership);
    return ops;
}

DeskOps::DeskOps(QObject *parent)
    : QObject(parent)
    , m_bin(new KCoreDirLister(this))
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &DeskOps::updateCut);
    updateCut();

    // Files writes its View settings without notifying, so the file itself
    // is watched.
    readView();
    KDirWatch::self()->addFile(filesConfigPath());
    connect(KDirWatch::self(), &KDirWatch::dirty, this, [this](const QString &path) {
        if (path == filesConfigPath()) {
            readView();
        }
    });
    connect(KDirWatch::self(), &KDirWatch::created, this, [this](const QString &path) {
        if (path == filesConfigPath()) {
            readView();
        }
    });

    // A quiet listing of the Recycle Bin keeps its desktop icon full or
    // empty.
    m_bin->setAutoErrorHandlingEnabled(false);
    m_bin->setShowHiddenFiles(true);
    auto recount = [this] {
        const int n = int(m_bin->items().size());
        if (n != m_binCount) {
            m_binCount = n;
            Q_EMIT binCountChanged();
        }
    };
    connect(m_bin, &KCoreDirLister::newItems, this, recount);
    connect(m_bin, &KCoreDirLister::itemsDeleted, this, recount);
    connect(m_bin, &KCoreDirLister::clear, this, recount);
    connect(m_bin, &KCoreDirLister::completed, this, recount);
    m_bin->openUrl(QUrl(QStringLiteral("trash:/")));
}

void DeskOps::readView()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosfilesrc"));
    config->reparseConfiguration();
    const KConfigGroup group(config, QStringLiteral("View"));
    const bool extensions = group.readEntry("ShowExtensions", false);
    const bool hidden = group.readEntry("ShowHidden", false);
    if (extensions != m_showExtensions || hidden != m_showHidden) {
        m_showExtensions = extensions;
        m_showHidden = hidden;
        Q_EMIT viewChanged();
    }
}

void DeskOps::ask(const QVariantMap &prompt, std::function<void(const QString &, bool)> reply)
{
    m_prompts.append(prompt);
    m_replies.append(std::move(reply));
    if (m_prompts.size() == 1) {
        Q_EMIT promptChanged();
    }
}

void DeskOps::answer(const QString &choice, bool applyToAll)
{
    if (m_prompts.isEmpty()) {
        return;
    }
    m_prompts.removeFirst();
    auto reply = m_replies.takeFirst();
    Q_EMIT promptChanged();
    reply(choice, applyToAll);
}

void DeskOps::watchJob(KJob *job, const QString &verb)
{
    if (auto tracker = KIO::getJobTracker()) {
        tracker->registerJob(job);
    }
    connect(job, &KJob::result, this, [this, job, verb] {
        if (!job->error() || job->error() == KIO::ERR_USER_CANCELED || job->error() == KJob::KilledJobError) {
            return;
        }
        QString message = job->errorString();
        switch (job->error()) {
        case KIO::ERR_ACCESS_DENIED:
        case KIO::ERR_WRITE_ACCESS_DENIED:
        case KIO::ERR_CANNOT_WRITE:
            message = QStringLiteral("You do not have permission to %1 here.").arg(verb);
            break;
        case KIO::ERR_DISK_FULL:
            message = QStringLiteral("There is not enough free space to %1 this.").arg(verb);
            break;
        case KIO::ERR_DOES_NOT_EXIST:
            message = QStringLiteral("The item is no longer there. It may have been moved or deleted.");
            break;
        default:
            break;
        }
        Q_EMIT failed(message);
    });
}

void DeskOps::open(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        // wildos: addresses (This computer) are pages inside Files, not
        // places KIO can reach, so they go straight to Files.
        if (url.scheme() == QLatin1String("wildos")) {
            auto job = new KIO::CommandLauncherJob(QStringLiteral("wildos-files"), {url.toString()});
            job->setDesktopName(QStringLiteral("org.wildos.files"));
            job->setUiDelegate(delegate());
            job->start();
            continue;
        }
        // A Windows program goes to the app that runs Windows programs (App
        // Bridge): KIO counts an .exe as something that would run itself and
        // refuses it outright rather than passing it to that app.
        const QMimeType type = QMimeDatabase().mimeTypeForUrl(url);
        if (type.inherits(QStringLiteral("application/x-ms-dos-executable")) || type.inherits(QStringLiteral("application/x-msi"))
            || type.inherits(QStringLiteral("application/x-msdownload")) || type.inherits(QStringLiteral("application/vnd.microsoft.portable-executable"))) {
            if (const KService::Ptr runner = KApplicationTrader::preferredService(type.name())) {
                auto job = new KIO::ApplicationLauncherJob(runner);
                job->setUrls({url});
                job->setUiDelegate(delegate());
                job->start();
                continue;
            }
        }
        // A program or script opens in its app rather than running, as in
        // Files; shortcuts (.desktop files) still start what they point at.
        auto job = new KIO::OpenUrlJob(url);
        job->setUiDelegate(delegate());
        // KIO only starts an app shortcut when executables may run; the
        // shortcut itself is still checked, and one that is not trusted
        // asks first (DeskUntrustedAsker).
        const bool shortcut = url.isLocalFile() && QMimeDatabase().mimeTypeForUrl(url).name() == QLatin1String("application/x-desktop");
        job->setRunExecutables(shortcut);
        reportLaunch(job);
        job->start();
    }
}

void DeskOps::openInFiles(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    const KService::Ptr files = KService::serviceByDesktopName(QStringLiteral("org.wildos.files"));
    if (!files) {
        open(urls);
        return;
    }
    // wildos: addresses are pages inside Files that KIO cannot reach, and
    // handing one to a launcher job sends it to kioexec to download, which
    // fails and leaves Files never opening. They go to Files as arguments.
    QStringList pages;
    for (const QUrl &url : urls) {
        if (url.scheme() == QLatin1String("wildos")) {
            pages.append(url.toString());
        }
    }
    if (!pages.isEmpty()) {
        QStringList arguments;
        for (const QUrl &url : urls) {
            arguments.append(url.scheme() == QLatin1String("wildos") ? url.toString() : url.toString(QUrl::PreferLocalFile));
        }
        auto job = new KIO::CommandLauncherJob(QStringLiteral("wildos-files"), arguments);
        job->setDesktopName(QStringLiteral("org.wildos.files"));
        job->setUiDelegate(delegate());
        job->start();
        return;
    }
    // One launch, one window with a tab for each folder, as Files opens
    // what it is given.
    auto job = new KIO::ApplicationLauncherJob(files);
    job->setUrls(urls);
    job->setUiDelegate(delegate());
    reportLaunch(job);
    job->start();
}

QVariantList DeskOps::appsFor(const QUrl &url) const
{
    QVariantList list;
    const QString mime = QMimeDatabase().mimeTypeForUrl(url).name();
    const auto services = KApplicationTrader::queryByMimeType(mime);
    for (const KService::Ptr &service : services) {
        if (!service->noDisplay() && list.size() < 8) {
            list.append(appEntry(service));
        }
    }
    return list;
}

QVariantList DeskOps::allApps() const
{
    QVariantList list;
    auto services = KApplicationTrader::query([](const KService::Ptr &service) {
        return !service->noDisplay() && !service->exec().isEmpty();
    });
    std::sort(services.begin(), services.end(), [](const KService::Ptr &a, const KService::Ptr &b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (const KService::Ptr &service : std::as_const(services)) {
        list.append(appEntry(service));
    }
    return list;
}

QString DeskOps::defaultAppName(const QUrl &url) const
{
    const QString mime = QMimeDatabase().mimeTypeForUrl(url).name();
    const KService::Ptr service = KApplicationTrader::preferredService(mime);
    return service ? service->name() : QString();
}

void DeskOps::openWith(const QList<QUrl> &urls, const QString &appId, bool makeDefault)
{
    const KService::Ptr service = KService::serviceByStorageId(appId);
    if (!service || urls.isEmpty()) {
        return;
    }
    if (makeDefault) {
        const QString mime = QMimeDatabase().mimeTypeForUrl(urls.constFirst()).name();
        KApplicationTrader::setPreferredService(mime, service);
    }
    auto job = new KIO::ApplicationLauncherJob(service);
    job->setUrls(urls);
    job->setUiDelegate(delegate());
    reportLaunch(job);
    job->start();
}

void DeskOps::openTerminal(const QUrl &folder)
{
    // The terminal chosen in kdeglobals: Wild OS sets its own, which is
    // started with --workdir. KTerminalLauncherJob only passes the folder to
    // Konsole, and falls back to Konsole whenever the TerminalService entry
    // is not in the app cache yet, so Wild OS Terminal is started directly.
    const QString path = folder.toLocalFile();
    const KConfigGroup general(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"));
    const bool ours = general.readEntry("TerminalService") == QLatin1String("org.wildos.terminal.desktop")
        || general.readEntry("TerminalApplication").section(QLatin1Char(' '), 0, 0).endsWith(QLatin1String("wildos-terminal"));
    if (ours && !QStandardPaths::findExecutable(QStringLiteral("wildos-terminal")).isEmpty()) {
        auto job = new KIO::CommandLauncherJob(QStringLiteral("wildos-terminal"), {QStringLiteral("--workdir"), path});
        job->setDesktopName(QStringLiteral("org.wildos.terminal"));
        job->setWorkingDirectory(path);
        reportLaunch(job);
        job->start();
        return;
    }
    auto job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(path);
    reportLaunch(job);
    job->start();
}

void DeskOps::openSettings(const QString &page)
{
    auto job = new KIO::CommandLauncherJob(QStringLiteral("wildos-settings"), {QStringLiteral("--page"), page});
    job->setDesktopName(QStringLiteral("org.wildos.settings"));
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            Q_EMIT failed(QStringLiteral("Settings could not be opened. It may not be installed."));
        }
    });
    job->start();
}

bool DeskOps::canPaste() const
{
    const QMimeData *data = QGuiApplication::clipboard()->mimeData();
    return data && data->hasUrls();
}

void DeskOps::updateCut()
{
    const QMimeData *data = QGuiApplication::clipboard()->mimeData();
    QSet<QUrl> cut;
    if (data && data->hasUrls() && KIO::isClipboardDataCut(data)) {
        const auto urls = KUrlMimeData::urlsFromMimeData(data);
        cut = QSet<QUrl>(urls.cbegin(), urls.cend());
    }
    m_cut = cut;
    Q_EMIT clipboardChanged();
}

void DeskOps::copy(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    auto data = new QMimeData;
    KUrlMimeData::setUrls(urls, urls, data);
    KIO::setClipboardDataCut(data, false);
    QGuiApplication::clipboard()->setMimeData(data);
}

void DeskOps::cut(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    auto data = new QMimeData;
    KUrlMimeData::setUrls(urls, urls, data);
    KIO::setClipboardDataCut(data, true);
    QGuiApplication::clipboard()->setMimeData(data);
}

void DeskOps::copyPaths(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const QUrl &url : urls) {
        paths.append(QLatin1Char('"') + (url.isLocalFile() ? url.toLocalFile() : url.toDisplayString()) + QLatin1Char('"'));
    }
    QGuiApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
}

void DeskOps::paste(const QUrl &folder)
{
    const QMimeData *data = QGuiApplication::clipboard()->mimeData();
    if (!data || !data->hasUrls()) {
        return;
    }
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(data);
    const bool move = KIO::isClipboardDataCut(data);
    transfer(urls, folder, move);
    if (move) {
        // A cut can only be pasted once, as on Windows.
        QGuiApplication::clipboard()->clear();
    }
}

int DeskOps::dropAction(const QList<QUrl> &urls, const QUrl &folder, int modifiers) const
{
    if (modifiers & Qt::ControlModifier) {
        return Qt::CopyAction;
    }
    if (modifiers & Qt::ShiftModifier) {
        return Qt::MoveAction;
    }
    if (urls.isEmpty() || !folder.isLocalFile() || folder.scheme() == QStringLiteral("trash")) {
        return folder.scheme() == QStringLiteral("trash") ? Qt::MoveAction : Qt::CopyAction;
    }
    // Windows moves within a drive and copies between drives.
    const QStorageInfo target(folder.toLocalFile());
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || QStorageInfo(url.toLocalFile()).rootPath() != target.rootPath()) {
            return Qt::CopyAction;
        }
    }
    return Qt::MoveAction;
}

void DeskOps::drop(const QList<QUrl> &urls, const QUrl &folder, int action)
{
    if (urls.isEmpty()) {
        return;
    }
    if (folder.scheme() == QStringLiteral("trash")) {
        trash(urls);
        return;
    }
    QList<QUrl> moving;
    for (const QUrl &url : urls) {
        // Dropping a folder onto itself, or an item where it already is,
        // does nothing.
        if (url == folder || KIO::upUrl(url).adjusted(QUrl::StripTrailingSlash) == folder.adjusted(QUrl::StripTrailingSlash)) {
            if (action == Qt::MoveAction) {
                continue;
            }
        }
        if (url.isParentOf(folder)) {
            continue;
        }
        moving.append(url);
    }
    if (!moving.isEmpty()) {
        transfer(moving, folder, action == Qt::MoveAction);
    }
}

void DeskOps::transfer(const QList<QUrl> &urls, const QUrl &folder, bool move)
{
    if (urls.isEmpty()) {
        return;
    }
    KIO::CopyJob *job = move ? KIO::move(urls, folder) : KIO::copy(urls, folder);
    job->setUiDelegate(delegate());
    bool sameFolder = !move;
    for (const QUrl &url : urls) {
        if (KIO::upUrl(url).adjusted(QUrl::StripTrailingSlash) != folder.adjusted(QUrl::StripTrailingSlash)) {
            sameFolder = false;
        }
    }
    job->setAutoRename(sameFolder);
    auto made = std::make_shared<QList<QUrl>>();
    connect(job, &KIO::CopyJob::copyingDone, this, [made](KIO::Job *, const QUrl &, const QUrl &to, const QDateTime &, bool, bool) {
        made->append(to);
    });
    connect(job, &KIO::CopyJob::copyingLinkDone, this, [made](KIO::Job *, const QUrl &, const QString &, const QUrl &to) {
        made->append(to);
    });
    connect(job, &KJob::result, this, [this, made, folder] {
        QList<QUrl> top;
        for (const QUrl &url : std::as_const(*made)) {
            if (KIO::upUrl(url).adjusted(QUrl::StripTrailingSlash) == folder.adjusted(QUrl::StripTrailingSlash)) {
                top.append(url);
            }
        }
        if (!top.isEmpty()) {
            Q_EMIT created(folder, top, false);
        }
    });
    watchJob(job, move ? QStringLiteral("move files") : QStringLiteral("copy files"));
}

void DeskOps::trash(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    QList<QUrl> local;
    QList<QUrl> other;
    for (const QUrl &url : urls) {
        (url.isLocalFile() ? local : other).append(url);
    }
    if (!local.isEmpty()) {
        auto job = KIO::trash(local);
        job->setUiDelegate(delegate());
        if (auto tracker = KIO::getJobTracker()) {
            tracker->registerJob(job);
        }
        connect(job, &KJob::result, this, [this, job, local] {
            if (!job->error() || job->error() == KIO::ERR_USER_CANCELED) {
                return;
            }
            QVariantMap prompt;
            prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
            prompt[QStringLiteral("title")] = QStringLiteral("Delete for good?");
            prompt[QStringLiteral("text")] = local.size() == 1
                ? QStringLiteral("This item cannot go to the Recycle Bin, so it would be deleted for good. You cannot undo this.")
                : QStringLiteral("These items cannot go to the Recycle Bin, so they would be deleted for good. You cannot undo this.");
            prompt[QStringLiteral("primary")] = QStringLiteral("Delete for good");
            ask(prompt, [this, local](const QString &choice, bool) {
                if (choice == QStringLiteral("primary")) {
                    auto del = KIO::del(local);
                    watchJob(del, QStringLiteral("delete files"));
                }
            });
        });
    }
    if (!other.isEmpty()) {
        deleteForGood(other);
    }
}

void DeskOps::deleteForGood(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = urls.size() == 1 ? QStringLiteral("Delete this item for good?") : QStringLiteral("Delete %1 items for good?").arg(urls.size());
    prompt[QStringLiteral("text")] = QStringLiteral("It will not go to the Recycle Bin, and you cannot undo this.");
    prompt[QStringLiteral("primary")] = QStringLiteral("Delete for good");
    ask(prompt, [this, urls](const QString &choice, bool) {
        if (choice == QStringLiteral("primary")) {
            auto job = KIO::del(urls);
            watchJob(job, QStringLiteral("delete files"));
        }
    });
}

void DeskOps::emptyBin()
{
    QVariantMap prompt;
    prompt[QStringLiteral("kind")] = QStringLiteral("confirm");
    prompt[QStringLiteral("title")] = QStringLiteral("Empty the Recycle Bin?");
    prompt[QStringLiteral("text")] = m_binCount == 1 ? QStringLiteral("The item in the Recycle Bin will be deleted for good. You cannot undo this.")
                                                     : QStringLiteral("All %1 items in the Recycle Bin will be deleted for good. You cannot undo this.").arg(m_binCount);
    prompt[QStringLiteral("primary")] = QStringLiteral("Empty Recycle Bin");
    ask(prompt, [this](const QString &choice, bool) {
        if (choice == QStringLiteral("primary")) {
            auto job = KIO::emptyTrash();
            watchJob(job, QStringLiteral("empty the Recycle Bin"));
        }
    });
}

QString DeskOps::editableName(const QString &fileName, bool isDir) const
{
    return Format::visibleName(fileName, isDir, m_showExtensions);
}

void DeskOps::rename(const QUrl &url, const QString &fileName, bool isDir, const QString &typed)
{
    // A shortcut shows the name written inside it, so renaming one changes
    // that name and leaves the file as it is.
    if (url.isLocalFile() && !isDir && fileName.endsWith(QLatin1String(".desktop"))) {
        const QString name = typed.trimmed();
        KDesktopFile desktop(url.toLocalFile());
        if (name.isEmpty() || name == desktop.readName()) {
            return;
        }
        if (!QFileInfo(url.toLocalFile()).isWritable()) {
            Q_EMIT failed(QStringLiteral("You do not have permission to rename this shortcut."));
            return;
        }
        desktop.desktopGroup().writeEntry("Name", name);
        desktop.sync();
        Q_EMIT created(KIO::upUrl(url), {url}, false);
        return;
    }
    const QString visible = Format::visibleName(fileName, isDir, m_showExtensions);
    QString name = typed.trimmed();
    if (name.isEmpty() || name == visible) {
        return;
    }
    if (name.contains(QLatin1Char('/')) || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        Q_EMIT failed(QStringLiteral("A name cannot contain / or be just dots. Try a different name."));
        return;
    }
    // The extension hidden from view stays on the file.
    name += fileName.mid(visible.size());
    const QUrl folder = KIO::upUrl(url);
    const QUrl target = childUrl(folder, name);
    if (target.isLocalFile() && QFileInfo::exists(target.toLocalFile()) && target.toLocalFile().compare(url.toLocalFile(), Qt::CaseSensitive) != 0) {
        Q_EMIT failed(QStringLiteral("There is already an item called \"%1\" here. Pick a different name.").arg(typed.trimmed()));
        return;
    }
    auto job = KIO::moveAs(url, target, KIO::HideProgressInfo);
    job->setUiDelegate(delegate());
    connect(job, &KJob::result, this, [this, job, folder, target] {
        if (!job->error()) {
            Q_EMIT created(folder, {target}, false);
        }
    });
    watchJob(job, QStringLiteral("rename this"));
}

QString DeskOps::uniqueName(const QUrl &folder, const QString &base, const QString &suffix) const
{
    if (!folder.isLocalFile()) {
        return base + suffix;
    }
    const QDir dir(folder.toLocalFile());
    QString name = base + suffix;
    for (int n = 2; dir.exists(name); ++n) {
        name = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(suffix);
    }
    return name;
}

void DeskOps::newFolder(const QUrl &folder)
{
    const QUrl url = childUrl(folder, uniqueName(folder, QStringLiteral("New folder"), QString()));
    auto job = KIO::mkdir(url);
    connect(job, &KJob::result, this, [this, job, folder, url] {
        if (!job->error()) {
            Q_EMIT created(folder, {url}, true);
        }
    });
    watchJob(job, QStringLiteral("make a folder"));
}

KJobUiDelegate *DeskOps::delegate()
{
    // KIO finds the question asker as a direct child of the job's delegate.
    auto ui = new KJobUiDelegate(KJobUiDelegate::Flags{});
    new DeskAsker(ui);
    new DeskOpenWithAsker(ui);
    new DeskUntrustedAsker(ui);
    return ui;
}

void DeskOps::reportLaunch(KJob *job)
{
    connect(job, &KJob::result, this, [this](KJob *job) {
        if (job->error() == 0 || job->error() == KIO::ERR_USER_CANCELED) {
            return;
        }
        QString message = job->errorString();
        if (job->error() == KIO::ERR_DOES_NOT_EXIST) {
            message = QStringLiteral("The item is no longer there. It may have been moved or deleted.");
        } else if (message.isEmpty()) {
            message = QStringLiteral("Wild OS could not open this.");
        }
        Q_EMIT failed(message);
    });
}

void DeskOps::bringForward(QWindow *window, QWindow *from)
{
    if (!window) {
        return;
    }
    if (KWindowSystem::isPlatformX11()) {
        KX11Extras::forceActiveWindow(window->winId());
        return;
    }
    if (KWindowSystem::isPlatformWayland() && from) {
        // The token is asked for on behalf of the desktop, which had the
        // click or key press that led here.
        const quint32 serial = KWaylandExtras::lastInputSerial(from);
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(KWaylandExtras::self(), &KWaylandExtras::xdgActivationTokenArrived, this,
                              [connection, serial, window = QPointer<QWindow>(window)](int arrived, const QString &token) {
                                  if (quint32(arrived) != serial) {
                                      return;
                                  }
                                  QObject::disconnect(*connection);
                                  if (window) {
                                      KWindowSystem::setCurrentXdgActivationToken(token);
                                      KWindowSystem::activateWindow(window);
                                  }
                              });
        KWaylandExtras::requestXdgActivationToken(from, serial, QString());
        return;
    }
    window->requestActivate();
}
