// wildos-recovery: start-up recovery (RecoveryStartup.dc,
// RecoveryStartupMore.dc). The recovery system's live session runs it
// alone in KWin, full screen, when started with wildos.mode=recovery.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-recovery"));
    app.setDesktopFileName(QStringLiteral("org.wildos.recovery"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.Recovery", "Main");
    return app.exec();
}
