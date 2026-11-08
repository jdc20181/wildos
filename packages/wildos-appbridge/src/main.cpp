#include "bridge.h"
#include "winapps.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTextStream>
#include <QUrl>

int main(int argc, char *argv[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wild OS App Bridge: opens Windows programs"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The Windows program or installer to open."));
    QCommandLineOption listOption(QStringLiteral("list"), QStringLiteral("Print the installed Windows apps as JSON."));
    QCommandLineOption tidyOption(QStringLiteral("tidy"), QStringLiteral("Remove the Start entries of Windows apps installed with Wine, which no longer runs."));
    QCommandLineOption removeOption(QStringLiteral("remove"), QStringLiteral("Remove the installed Windows app with this id."), QStringLiteral("id"));
    QCommandLineOption backdropOption(QStringLiteral("backdrop"), QStringLiteral("Draw the dialog over this picture in a 1440 by 900 window, for previews."),
                                      QStringLiteral("image"));
    parser.addOptions({listOption, removeOption, tidyOption, backdropOption});

    // The command-line modes run without a window, so they are handled
    // before a GUI application exists.
    {
        QCoreApplication core(argc, argv);
        core.setApplicationName(QStringLiteral("wildos-appbridge"));
        core.setApplicationVersion(QStringLiteral(WILDOS_APPBRIDGE_VERSION));
        parser.process(core);
        if (parser.isSet(listOption)) {
            QTextStream(stdout) << QJsonDocument(WinApps::list()).toJson();
            return 0;
        }
        if (parser.isSet(tidyOption)) {
            WinApps::tidy();
            return 0;
        }
        if (parser.isSet(removeOption)) {
            return WinApps::remove(parser.value(removeOption)) ? 0 : 1;
        }
        if (parser.positionalArguments().isEmpty()) {
            parser.showHelp(1);
        }
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-appbridge"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Windows app"));
    app.setApplicationVersion(QStringLiteral(WILDOS_APPBRIDGE_VERSION));
    app.setDesktopFileName(QStringLiteral("org.wildos.appbridge"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("wildos-windows-app")));

    QString file = parser.positionalArguments().constFirst();
    if (file.startsWith(QLatin1String("file:"))) {
        file = QUrl(file).toLocalFile();
    }
    Bridge::setFile(file);

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlApplicationEngine engine;
    const QString backdrop = parser.value(backdropOption);
    engine.setInitialProperties({
        {QStringLiteral("backdrop"), backdrop.isEmpty() ? QString() : QUrl::fromLocalFile(QFileInfo(backdrop).absoluteFilePath()).toString()},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.AppBridge", "Main");
    return app.exec();
}
