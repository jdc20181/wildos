// wildos-winmode-setup: the Windows mode setup wizard, boards 9.1 to 9.7.
// The setup itself runs in the system service (winmoded), so closing this
// window never stops it; opening it again shows where it has got to. The
// checks and choices live in WildOS.WinMode.Setup, which first run shares.

#include <QCommandLineParser>
#include <QFileInfo>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char *argv[])
{
    // A QApplication, not QGuiApplication: the KDE platform theme only gives
    // widget applications its file dialog, and Qt Quick otherwise draws its
    // own light, out-of-place one.
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-winmode-setup"));
    app.setOrganizationDomain(QStringLiteral("wildos.io"));
    app.setApplicationDisplayName(QStringLiteral("Windows mode"));
    app.setDesktopFileName(QStringLiteral("org.wildos.winmode"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("wildos-windows-mode")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Set up Windows mode"));
    parser.addHelpOption();
    const QCommandLineOption previewOption(QStringLiteral("preview"),
                                           QStringLiteral("Show a board with made-up values, for design reviews: check, short, home, nolicence, options, setup, download, ready or problems."),
                                           QStringLiteral("board"));
    const QCommandLineOption backdropOption(QStringLiteral("backdrop"), QStringLiteral("Draw the window over this picture, for previews."),
                                            QStringLiteral("image"));
    parser.addOptions({previewOption, backdropOption});
    parser.process(app);

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlApplicationEngine engine;
    const QString backdrop = parser.value(backdropOption);
    engine.setInitialProperties({
        {QStringLiteral("backdrop"), backdrop.isEmpty() ? QString() : QUrl::fromLocalFile(QFileInfo(backdrop).absoluteFilePath()).toString()},
        {QStringLiteral("board"), parser.value(previewOption)},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("WildOS.WinMode", "Main");
    return app.exec();
}
