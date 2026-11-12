#include "musicapp.h"
#include "player.h"

#include <KWindowSystem>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDebug>
#include <QDir>
#include <QQmlComponent>
#include <QTemporaryFile>
#include <QTimer>

namespace
{
MusicApp *s_instance = nullptr;

QDBusMessage kwinCall(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, interface, method);
    message.setArguments(arguments);
    return message;
}

// On Wayland a window cannot put itself above the others; KWin can, so a
// short script asks it to for the mini player.
constexpr const char keepAboveScript[] = R"(
for (const w of workspace.windowList()) {
    if (w.pid === %1 && w.caption === %2) {
        w.keepAbove = true;
    }
}
)";
}

MusicApp::MusicApp(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

MusicApp *MusicApp::instance()
{
    if (!s_instance) {
        new MusicApp(QCoreApplication::instance());
    }
    return s_instance;
}

MusicApp *MusicApp::create(QQmlEngine *, QJSEngine *)
{
    MusicApp *app = instance();
    QQmlEngine::setObjectOwnership(app, QQmlEngine::CppOwnership);
    return app;
}

void MusicApp::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

void MusicApp::setMainWindow(QQuickWindow *window)
{
    m_main = window;
}

bool MusicApp::mini() const
{
    return m_mini && m_mini->isVisible();
}

void MusicApp::handle(const QStringList &arguments, const QString &workingDirectory)
{
    const QDir dir(workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory);
    QStringList files;
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const QString &arg = arguments.at(i);
        if (arg.startsWith(QLatin1Char('-'))) {
            continue;
        }
        files.append(arg.contains(QLatin1String("://")) ? arg : dir.absoluteFilePath(arg));
    }
    if (!files.isEmpty()) {
        Player::instance()->openFiles(files);
    }
    if (mini()) {
        activate(m_mini);
    } else if (m_main) {
        activate(m_main);
    }
}

void MusicApp::showMini()
{
    if (!m_mini && m_engine) {
        QQmlComponent component(m_engine, QStringLiteral("WildOS.Music"), QStringLiteral("MiniPlayer"));
        QObject *object = component.create();
        auto *window = qobject_cast<QQuickWindow *>(object);
        if (!window) {
            qWarning().noquote() << component.errorString();
            delete object;
            return;
        }
        QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
        window->QObject::setParent(this);
        connect(window, &QWindow::visibleChanged, this, &MusicApp::miniChanged);
        m_mini = window;
    }
    if (!m_mini) {
        return;
    }
    if (m_main) {
        m_main->hide();
    }
    activate(m_mini);
    keepAbove(m_mini);
    Q_EMIT miniChanged();
}

void MusicApp::showMain()
{
    if (m_main) {
        activate(m_main);
    }
    if (m_mini) {
        m_mini->hide();
    }
    Q_EMIT miniChanged();
}

void MusicApp::activate(QQuickWindow *window)
{
    window->show();
    window->raise();
    // A start from the launcher carries an activation token, which lets the
    // window take the focus from the app in front.
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
    window->requestActivate();
}

void MusicApp::keepAbove(QQuickWindow *window)
{
    if (!KWindowSystem::isPlatformWayland()) {
        return;
    }
    const QString title = window->title();
    // The window has to be mapped before KWin can find it.
    QTimer::singleShot(250, this, [title] {
        auto *file = new QTemporaryFile(QDir::temp().filePath(QStringLiteral("wildos-music-XXXXXX.js")));
        if (!file->open()) {
            delete file;
            return;
        }
        QString quoted = title;
        quoted.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
        file->write(QString::fromLatin1(keepAboveScript).arg(QCoreApplication::applicationPid()).arg(QLatin1Char('"') + quoted + QLatin1Char('"')).toUtf8());
        file->flush();

        const QString plugin = QStringLiteral("wildos-music-keep-above");
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.call(kwinCall(QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("unloadScript"), {plugin}), QDBus::Block, 500);
        const QDBusMessage loaded = bus.call(kwinCall(QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("loadScript"),
                                                      {file->fileName(), plugin}),
                                             QDBus::Block, 500);
        const int id = loaded.arguments().value(0).toInt();
        if (loaded.type() == QDBusMessage::ReplyMessage && id >= 0) {
            bus.call(kwinCall(QStringLiteral("/Scripting/Script%1").arg(id), QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run"), {}), QDBus::Block, 500);
        }
        // KWin has read the file once the script runs; it is unloaded when
        // it has had time to.
        QTimer::singleShot(2000, [file, plugin] {
            QDBusConnection::sessionBus().asyncCall(
                kwinCall(QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("unloadScript"), {plugin}));
            delete file;
        });
    });
}
