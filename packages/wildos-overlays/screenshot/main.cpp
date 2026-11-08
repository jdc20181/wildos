#include "frozenimage.h"
#include "screenshotapp.h"

#include <KDBusService>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

int main(int argc, char *argv[])
{
    // Widgets only for Purpose's share plugins, some of which show a dialog.
    QApplication app(argc, argv);
    app.setOrganizationDomain(QStringLiteral("wildos.org"));
    app.setApplicationName(QStringLiteral("screenshot"));
    app.setApplicationDisplayName(QStringLiteral("Screenshot"));
    app.setDesktopFileName(QStringLiteral("org.wildos.screenshot"));
    app.setQuitOnLastWindowClosed(false);
    // It decides itself when to end; finished background jobs (previews,
    // share plugins) must not end it while it waits hidden.
    app.setQuitLockEnabled(false);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQuickWindow::setDefaultAlphaBuffer(true);

    // The service name is org.wildos.screenshot, which KWin's window list
    // script calls back on.
    KDBusService service(KDBusService::Unique);

    ScreenshotApp screenshot;
    QObject::connect(&service, &KDBusService::activateRequested, &screenshot, [&screenshot](const QStringList &arguments, const QString &) {
        screenshot.handle(arguments);
    });

    // The one instance, which D-Bus activation also reaches.
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ScreenshotApp"), &screenshot);
    engine.addImageProvider(QStringLiteral("frozen"), screenshot.frozen());
    engine.loadFromModule("WildOS.Screenshot", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    screenshot.handle(app.arguments());
    return app.exec();
}
