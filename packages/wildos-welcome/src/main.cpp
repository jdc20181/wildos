#include "background.h"
#include "firstrun.h"
#include "setup.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-welcome"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Welcome"));
    app.setApplicationVersion(QStringLiteral(WILDOS_WELCOME_VERSION));
    app.setDesktopFileName(QStringLiteral("org.wildos.welcome"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("wildos")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wild OS first-run welcome"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption autostartOption(QStringLiteral("autostart"), QStringLiteral("Started at sign-in: only show if this account has not been set up yet."));
    QCommandLineOption copyOption(QStringLiteral("copy"), QStringLiteral("Copy files from Windows as described in this job file."), QStringLiteral("file"));
    QCommandLineOption installOption(QStringLiteral("install"), QStringLiteral("Install the apps listed in this job file."), QStringLiteral("file"));
    QCommandLineOption windowedOption(QStringLiteral("windowed"), QStringLiteral("Show in a 1440 by 900 window rather than over the whole screen."));
    QCommandLineOption stepOption(QStringLiteral("step"), QStringLiteral("Open on this step, 1 to 7 with --setup, else 1 to 4."), QStringLiteral("number"));
    QCommandLineOption setupOption(QStringLiteral("setup"),
                                   QStringLiteral("Run the first boot setup session, all seven steps, before any account exists. Started by wildos-setup."));
    QCommandLineOption programsOption(QStringLiteral("programs"), QStringLiteral("Fetch the Windows installers listed in this job file."), QStringLiteral("file"));
    parser.addOptions({autostartOption, copyOption, installOption, programsOption, windowedOption, stepOption, setupOption});
    parser.process(app);

    if (parser.isSet(copyOption)) {
        return Background::runCopy(parser.value(copyOption));
    }
    if (parser.isSet(installOption)) {
        return Background::runInstall(parser.value(installOption));
    }
    if (parser.isSet(programsOption)) {
        return FirstRun::runPrograms(parser.value(programsOption));
    }

    // An account set up in the first boot session has its choices waiting;
    // they are applied as it signs in, with no window.
    if (parser.isSet(autostartOption) && FirstRun::hasPlan()) {
        return FirstRun::apply();
    }

    if (parser.isSet(autostartOption)) {
        const QString reason = Setup::reasonNotToRun();
        if (!reason.isEmpty() || Setup::alreadyDone()) {
            if (!reason.isEmpty()) {
                QTextStream(stderr) << "wildos-welcome: not showing, " << reason << Qt::endl;
            }
            return 0;
        }
    }

    // One welcome at a time; a second start from Start or the autostart
    // entry would otherwise stack a second sheet on top.
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(QStringLiteral("org.wildos.Welcome"))) {
        return 0;
    }

    Setup::setPlanning(parser.isSet(setupOption));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("windowed"), parser.isSet(windowedOption)},
        {QStringLiteral("setupMode"), parser.isSet(setupOption)},
        {QStringLiteral("startStep"), qBound(1, parser.value(stepOption).toInt(), 7)},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.Welcome", "Main");
    return app.exec();
}
