#include "archiveapp.h"

#include "format.h"

#include <KWindowSystem>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QQmlComponent>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

namespace
{
ArchiveApp *s_instance = nullptr;
constexpr int recentLimit = 8;

QStringList filesFrom(const QStringList &arguments, const QString &workingDirectory)
{
    const QDir dir(workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory);
    QStringList files;
    for (const QString &arg : arguments) {
        const QUrl url = QUrl::fromUserInput(arg, dir.absolutePath(), QUrl::AssumeLocalFile);
        if (url.isLocalFile()) {
            files.append(QFileInfo(url.toLocalFile()).absoluteFilePath());
        }
    }
    return files;
}

struct Command {
    QString mode;
    QStringList files;
};

Command parse(const QStringList &arguments, const QString &workingDirectory)
{
    QCommandLineParser parser;
    const QCommandLineOption extractHere(QStringLiteral("extract-here"), QStringLiteral("Extract each archive beside it."));
    const QCommandLineOption compress(QStringLiteral("compress"), QStringLiteral("Make a new archive of these files and folders."));
    parser.addOption(extractHere);
    parser.addOption(compress);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Archives to open."), QStringLiteral("[files...]"));
    parser.parse(arguments);
    Command command;
    command.files = filesFrom(parser.positionalArguments(), workingDirectory);
    command.mode = parser.isSet(extractHere) ? QStringLiteral("extract-here") : parser.isSet(compress) ? QStringLiteral("compress") : QStringLiteral("open");
    return command;
}

QVariantList urlsOf(const QStringList &files)
{
    QVariantList urls;
    for (const QString &file : files) {
        urls.append(QUrl::fromLocalFile(file));
    }
    return urls;
}

QString openedRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/opened");
}
}

ArchiveApp::ArchiveApp(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    // Copies made to open files from archives are kept for the app that
    // opened them until the next start.
    QDir(openedRoot()).removeRecursively();
}

ArchiveApp *ArchiveApp::instance()
{
    if (!s_instance) {
        new ArchiveApp(QCoreApplication::instance());
    }
    return s_instance;
}

ArchiveApp *ArchiveApp::create(QQmlEngine *, QJSEngine *)
{
    ArchiveApp *app = instance();
    QQmlEngine::setObjectOwnership(app, QQmlEngine::CppOwnership);
    return app;
}

void ArchiveApp::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

void ArchiveApp::adopt(QQuickWindow *window)
{
    if (window) {
        m_windows.append(window);
    }
}

QVariantMap ArchiveApp::firstWindow(const QStringList &arguments, const QString &workingDirectory)
{
    const Command command = parse(arguments, workingDirectory);
    QVariantMap properties;
    if (command.mode == QLatin1String("compress")) {
        properties.insert(QStringLiteral("compressUrls"), urlsOf(command.files));
    } else if (command.mode == QLatin1String("open") && !command.files.isEmpty()) {
        properties.insert(QStringLiteral("startPath"), command.files.first());
    }
    return properties;
}

void ArchiveApp::handle(const QStringList &arguments, const QString &workingDirectory, bool first)
{
    const Command command = parse(arguments, workingDirectory);
    if (command.mode == QLatin1String("extract-here")) {
        for (const QString &file : command.files) {
            makeWindow(QStringLiteral("ExtractHereWindow"), {{QStringLiteral("archivePath"), file}});
        }
        quitIfDone();
        return;
    }
    if (first) {
        // The first window took the first archive; the others get windows
        // of their own.
        for (int i = command.mode == QLatin1String("open") ? 1 : int(command.files.size()); i < command.files.size(); ++i) {
            openWindow(command.files[i]);
        }
        return;
    }
    if (command.mode == QLatin1String("compress")) {
        activate(makeWindow(QStringLiteral("Main"), {{QStringLiteral("compressUrls"), urlsOf(command.files)}}));
        return;
    }
    if (command.files.isEmpty()) {
        for (const QPointer<QQuickWindow> &window : std::as_const(m_windows)) {
            if (window && window->objectName() == QLatin1String("main")) {
                activate(window);
                return;
            }
        }
        activate(makeWindow(QStringLiteral("Main"), {}));
        return;
    }
    for (const QString &file : command.files) {
        openWindow(file);
    }
}

void ArchiveApp::openWindow(const QString &path)
{
    // A window still on its start view takes the archive.
    for (const QPointer<QQuickWindow> &window : std::as_const(m_windows)) {
        if (window && window->objectName() == QLatin1String("main") && window->property("idle").toBool()) {
            QMetaObject::invokeMethod(window, "openPath", Q_ARG(QVariant, path));
            activate(window);
            return;
        }
    }
    activate(makeWindow(QStringLiteral("Main"), {{QStringLiteral("startPath"), path}}));
}

QQuickWindow *ArchiveApp::makeWindow(const QString &type, const QVariantMap &properties)
{
    if (!m_engine) {
        return nullptr;
    }
    QQmlComponent component(m_engine, QStringLiteral("WildOS.Archive"), type);
    QObject *object = component.createWithInitialProperties(properties);
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning().noquote() << component.errorString();
        delete object;
        return nullptr;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    static_cast<QObject *>(window)->setParent(m_engine);
    m_windows.append(window);
    return window;
}

void ArchiveApp::activate(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    window->show();
    window->raise();
    // Focus stealing prevention needs the activation token the launcher
    // or Files handed over with the request.
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

void ArchiveApp::windowClosed(QObject *window)
{
    m_windows.removeAll(nullptr);
    m_windows.removeAll(QPointer<QQuickWindow>(qobject_cast<QQuickWindow *>(window)));
    window->deleteLater();
    QTimer::singleShot(0, this, &ArchiveApp::quitIfDone);
}

void ArchiveApp::quitIfDone()
{
    m_windows.removeAll(nullptr);
    if (m_windows.isEmpty()) {
        QCoreApplication::quit();
    }
}

QVariantList ArchiveApp::recent() const
{
    QSettings settings;
    const QStringList paths = settings.value(QStringLiteral("Recent/archives")).toStringList();
    QVariantList list;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile()) {
            continue;
        }
        list.append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("folder"), Format::folder(info.absolutePath())},
            {QStringLiteral("size"), Format::size(info.size())},
            {QStringLiteral("modified"), Format::date(info.lastModified())},
        });
    }
    return list;
}

void ArchiveApp::addRecent(const QString &path)
{
    QSettings settings;
    QStringList paths = settings.value(QStringLiteral("Recent/archives")).toStringList();
    paths.removeAll(path);
    paths.prepend(path);
    while (paths.size() > recentLimit) {
        paths.removeLast();
    }
    settings.setValue(QStringLiteral("Recent/archives"), paths);
    Q_EMIT recentChanged();
}

void ArchiveApp::removeRecent(const QString &path)
{
    QSettings settings;
    QStringList paths = settings.value(QStringLiteral("Recent/archives")).toStringList();
    paths.removeAll(path);
    settings.setValue(QStringLiteral("Recent/archives"), paths);
    Q_EMIT recentChanged();
}

QString ArchiveApp::openingFolder()
{
    const QString dir = openedRoot() + QLatin1Char('/') + QUuid::createUuid().toString(QUuid::Id128).left(12);
    if (!QDir().mkpath(dir)) {
        return QString();
    }
    QFile::setPermissions(openedRoot(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return dir;
}

void ArchiveApp::showFolder(const QString &path)
{
    // Opened as a folder rather than through org.freedesktop.FileManager1:
    // Kicksecure's fm-shim-backend holds that name and shows nothing.
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
