#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>

// Shows every WildOS.UI control in every state, for design review and for
// checking a change in the VM. The scheme follows the system unless
// WILDOS_UI_THEME=light or dark is set.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-ui-gallery"));
    app.setApplicationDisplayName(QStringLiteral("Wild OS controls"));
    app.setDesktopFileName(QStringLiteral("org.wildos.uigallery"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("controls or overlays"), QStringLiteral("page"), QStringLiteral("controls"));
    QCommandLineOption sizeOption(QStringLiteral("text-size"), QStringLiteral("System font size in points (10 is the default)"), QStringLiteral("points"));
    // Changes the system font two seconds after start, the way Settings'
    // text size does, to check that running windows follow it.
    QCommandLineOption laterOption(QStringLiteral("text-size-later"), QStringLiteral("Change the font size after two seconds"), QStringLiteral("points"));
    parser.addOption(pageOption);
    parser.addOption(sizeOption);
    parser.addOption(laterOption);
    parser.addPositionalArgument(QStringLiteral("qml"), QStringLiteral("Gallery.qml to load"));
    parser.process(app);

    QFont font(QStringLiteral("Albert Sans"));
    font.setPointSizeF(parser.isSet(sizeOption) ? parser.value(sizeOption).toDouble() : 10.0);
    QGuiApplication::setFont(font);

    if (parser.isSet(laterOption)) {
        const double later = parser.value(laterOption).toDouble();
        QTimer::singleShot(2000, &app, [later] {
            QFont changed = QGuiApplication::font();
            changed.setPointSizeF(later);
            QGuiApplication::setFont(changed);
        });
    }

    QQmlApplicationEngine engine;
#ifdef WILDOS_UI_BUILD_QML
    if (QFileInfo::exists(QStringLiteral(WILDOS_UI_BUILD_QML "/WildOS/UI/qmldir"))) {
        engine.addImportPath(QStringLiteral(WILDOS_UI_BUILD_QML));
    }
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("galleryPage"), parser.value(pageOption));
    // A wallpaper behind the glass and a photo for the avatar make the
    // review honest; without them the gallery uses the lake navy ground.
    const auto fileUrl = [](const char *variable) {
        const QString path = qEnvironmentVariable(variable);
        return QFileInfo::exists(path) ? QUrl::fromLocalFile(path) : QUrl();
    };
    engine.rootContext()->setContextProperty(QStringLiteral("galleryWallpaper"), fileUrl("WILDOS_GALLERY_WALLPAPER"));
    engine.rootContext()->setContextProperty(QStringLiteral("galleryPhoto"), fileUrl("WILDOS_GALLERY_PHOTO"));

    QString file = parser.positionalArguments().value(0);
    if (file.isEmpty()) {
        file = QStringLiteral("/usr/share/wildos-ui/Gallery.qml");
#ifdef WILDOS_UI_GALLERY_QML
        if (!QFileInfo::exists(file)) {
            file = QStringLiteral(WILDOS_UI_GALLERY_QML);
        }
#endif
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath()));
    return app.exec();
}
