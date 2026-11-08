#include "catalog.h"
#include "themeiconprovider.h"

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
const QString serviceName = QStringLiteral("org.wildos.Store");
const QString objectPath = QStringLiteral("/Store");
const QString interfaceName = QStringLiteral("org.wildos.Store");
}

// A second "wildos-store --app x" (from Start, Welcome or the .exe handler)
// brings the open Store forward on that app instead of opening another one.
class Remote : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Store")

public:
    explicit Remote(QQmlApplicationEngine *engine)
        : QObject(engine)
        , m_engine(engine)
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE void show(const QString &app, const QString &search, const QString &page)
    {
        const auto roots = m_engine->rootObjects();
        if (roots.isEmpty()) {
            return;
        }
        QObject *root = roots.constFirst();
        QMetaObject::invokeMethod(root, "openTarget", Q_ARG(QVariant, app), Q_ARG(QVariant, search), Q_ARG(QVariant, page));
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
    app.setApplicationName(QStringLiteral("wildos-store"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Store"));
    app.setApplicationVersion(QStringLiteral(WILDOS_STORE_VERSION));
    app.setDesktopFileName(QStringLiteral("org.wildos.store"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.wildos.store"), QIcon::fromTheme(QStringLiteral("plasmadiscover"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wild OS Store"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption appOption(QStringLiteral("app"), QStringLiteral("Open the page of this app (an AppStream ID, package or Flathub ID)."),
                                 QStringLiteral("id"));
    QCommandLineOption searchOption(QStringLiteral("search"), QStringLiteral("Search for this text."), QStringLiteral("text"));
    QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("Open home, apps, games, windows, updates or installed."), QStringLiteral("page"));
    parser.addOption(appOption);
    parser.addOption(searchOption);
    parser.addOption(pageOption);
    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("An appstream: link."), QStringLiteral("[url]"));
    parser.process(app);

    QString target = parser.value(appOption);
    const QStringList positional = parser.positionalArguments();
    if (target.isEmpty() && !positional.isEmpty()) {
        target = positional.constFirst();
    }
    const QString search = parser.value(searchOption);
    const QString page = parser.value(pageOption);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && bus.interface()->isServiceRegistered(serviceName)) {
        auto message = QDBusMessage::createMethodCall(serviceName, objectPath, interfaceName, QStringLiteral("show"));
        message << target << search << page;
        bus.call(message, QDBus::Block, 5000);
        return 0;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("theme"), new ThemeIconProvider);
    engine.setInitialProperties({
        {QStringLiteral("startApp"), target},
        {QStringLiteral("startSearch"), search},
        {QStringLiteral("startPage"), page.isEmpty() ? QStringLiteral("home") : page},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.Store", "Main");

    if (bus.isConnected()) {
        auto remote = new Remote(&engine);
        bus.registerObject(objectPath, remote, QDBusConnection::ExportScriptableSlots);
        bus.registerService(serviceName);
    }

    return app.exec();
}

#include "main.moc"
