#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>

namespace
{
const QString serviceName = QStringLiteral("org.wildos.TaskManager");
const QString objectPath = QStringLiteral("/TaskManager");
}

// Ctrl+Shift+Esc a second time brings the open Task Manager forward, on the
// page asked for, instead of opening another, as Windows does.
class Remote : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.TaskManager")

public:
    explicit Remote(QQmlApplicationEngine *engine)
        : QObject(engine)
        , m_engine(engine)
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE void show(const QString &page)
    {
        const auto roots = m_engine->rootObjects();
        if (roots.isEmpty()) {
            return;
        }
        QObject *root = roots.constFirst();
        if (!page.isEmpty()) {
            QMetaObject::invokeMethod(root, "showPage", Q_ARG(QVariant, page));
        }
        if (auto window = qobject_cast<QQuickWindow *>(root)) {
            if (window->visibility() == QWindow::Minimized) {
                window->showNormal();
            }
            window->show();
            window->raise();
            window->requestActivate();
        }
    }

private:
    QQmlApplicationEngine *m_engine;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-taskmanager"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Task Manager"));
    app.setApplicationVersion(QStringLiteral(WILDOS_TASKMANAGER_VERSION));
    app.setDesktopFileName(QStringLiteral("org.wildos.taskmanager"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.wildos.taskmanager")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wild OS Task Manager"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption pageOption(QStringLiteral("page"),
                                  QStringLiteral("Open on this page: processes, performance, startup, windowsapps, services or settings."),
                                  QStringLiteral("page"));
    parser.addOption(pageOption);
    parser.process(app);
    const QString page = parser.value(pageOption);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && bus.interface()->isServiceRegistered(serviceName)) {
        auto message = QDBusMessage::createMethodCall(serviceName, objectPath, serviceName, QStringLiteral("show"));
        message << page;
        bus.call(message, QDBus::Block, 5000);
        return 0;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    // The window draws its own title bar and rounded glass sheet.
    QQuickWindow::setDefaultAlphaBuffer(true);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("requestedPage"), page}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.TaskManager", "Main");

    if (bus.isConnected()) {
        bus.registerObject(objectPath, new Remote(&engine), QDBusConnection::ExportScriptableSlots);
        bus.registerService(serviceName);
    }
    return app.exec();
}

#include "main.moc"
