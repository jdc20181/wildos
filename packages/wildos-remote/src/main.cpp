// wildos-remote: Remote, Wild OS's app for other computers. Remote Desktop
// to Windows PCs and servers on the Windows mode engine, and SSH to
// servers with a terminal, files and a keychain.
//
//   wildos-remote [ssh://user@host:port | rdp://host | user@host]
//   wildos-remote /args-from:stdin
//
// The second is how Windows mode opens the whole Windows desktop with
// Remote: FreeRDP's arguments as xfreerdp3 takes them, one per line on
// standard input with the password last, so nothing secret is ever on a
// command line. Only that desktop's window opens.
//
// ssh, ssh-keygen and ssh-add also run this binary as their SSH_ASKPASS;
// it then only passes the question back to the Remote that started them.

#include "app.h"
#include "askpass.h"
#include "remotedesktop.h"
#include "sessionwindow.h"

#include <QFile>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>

namespace
{
const QString serviceName = QStringLiteral("org.wildos.Remote");
const QString objectPath = QStringLiteral("/Remote");
}

// A second launch (or a link) goes to the Remote already running.
class Bus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Remote")

public:
    using QObject::QObject;

public Q_SLOTS:
    Q_SCRIPTABLE void open(const QString &arg)
    {
        App::instance()->openArgument(arg);
    }
};

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsSet("WILDOS_REMOTE_ASKPASS_TOKEN") && argc >= 1) {
        QCoreApplication helper(argc, argv);
        return AskPass::ask(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString());
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-remote"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Remote"));
    app.setApplicationVersion(QStringLiteral(WILDOS_REMOTE_VERSION));
    app.setDesktopFileName(QStringLiteral("org.wildos.remote"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.wildos.remote")));
    // Closing the home window while a terminal or desktop is open leaves
    // those running.
    app.setQuitOnLastWindowClosed(true);

    if (app.arguments().contains(QStringLiteral("/args-from:stdin"))) {
        QList<QByteArray> args;
        QString title = QStringLiteral("Windows desktop");
        QFile in;
        if (in.open(stdin, QIODevice::ReadOnly)) {
            while (!in.atEnd()) {
                QByteArray line = in.readLine();
                while (line.endsWith('\n') || line.endsWith('\r')) {
                    line.chop(1);
                }
                if (line.startsWith("/title:")) {
                    title = QString::fromUtf8(line.mid(7));
                } else if (!line.isEmpty()) {
                    args.append(line);
                }
            }
        }
        if (args.isEmpty()) {
            return 1;
        }
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QQuickWindow::setDefaultAlphaBuffer(true);
        App::instance()->setDesktopOnly(true);
        QQmlApplicationEngine engine;
        engine.loadFromModule("WildOS.Remote", "Main");
        Desktops::instance()->startWithArguments(title, args);
        for (QByteArray &a : args) {
            a.fill('\0');
        }
        const int status = app.exec();
        Desktops::instance()->closeAll();
        return status;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wild OS Remote"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("address"), QStringLiteral("An ssh:// or rdp:// link, or user@host, to connect to."));
    parser.process(app);
    const QString arg = parser.positionalArguments().value(0);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && bus.interface()->isServiceRegistered(serviceName)) {
        auto message = QDBusMessage::createMethodCall(serviceName, objectPath, serviceName, QStringLiteral("open"));
        message << arg;
        bus.call(message);
        return 0;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQuickWindow::setDefaultAlphaBuffer(true);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.Remote", "Main");

    auto *exported = new Bus(&app);
    if (bus.isConnected()) {
        bus.registerObject(objectPath, exported, QDBusConnection::ExportScriptableSlots);
        bus.registerService(serviceName);
    }
    if (!arg.isEmpty()) {
        QMetaObject::invokeMethod(App::instance(), [arg] { App::instance()->openArgument(arg); }, Qt::QueuedConnection);
    }
    const int status = app.exec();
    Desktops::instance()->closeAll();
    // The terminal window's QML goes before the engines and the
    // application it depends on.
    delete SessionWindow::instance(false);
    return status;
}

#include "main.moc"
