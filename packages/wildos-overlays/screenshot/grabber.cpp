#include "grabber.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QPixmap>
#include <QScreen>
#include <QTemporaryFile>
#include <QTimer>

#include <fcntl.h>
#include <unistd.h>

namespace
{
const QString kwinService = QStringLiteral("org.kde.KWin.ScreenShot2");
const QString kwinPath = QStringLiteral("/org/kde/KWin/ScreenShot2");
const QString kwinInterface = QStringLiteral("org.kde.KWin.ScreenShot2");

// Normal windows and dialogs on this desktop, not the panel, the desktop
// or our own overlay. stackingOrder runs bottom to top.
const char *windowScript = R"JS(
const out = [];
const list = workspace.stackingOrder;
for (let i = 0; i < list.length; i++) {
    const w = list[i];
    if (!(w.normalWindow || w.dialog) || w.minimized || w.deleted) {
        continue;
    }
    if (!(w.onAllDesktops || w.desktops.indexOf(workspace.currentDesktop) >= 0)) {
        continue;
    }
    const g = w.frameGeometry;
    out.push({id: w.internalId.toString(), x: g.x, y: g.y, width: g.width, height: g.height, caption: w.caption});
}
callDBus("org.wildos.screenshot", "/WindowList", "org.wildos.Screenshot", "setWindows", JSON.stringify(out));
)JS";
}

Grabber::Grabber(QObject *parent)
    : QObject(parent)
{
    new WindowListReceiver(this);
}

QImage Grabber::callKWin(const QString &method, const QVariantList &arguments)
{
    if (!m_kwinWorks || !QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        return {};
    }
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        return {};
    }
    QDBusMessage message = QDBusMessage::createMethodCall(kwinService, kwinPath, kwinInterface, method);
    QVariantList args = arguments;
    args.append(QVariant::fromValue(QDBusUnixFileDescriptor(fds[1])));
    message.setArguments(args);
    ::close(fds[1]);

    // KWin replies with the picture's layout first and then writes the
    // pixels into the pipe from its own thread, so reading after the reply
    // cannot deadlock.
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (reply.errorName() == QLatin1String("org.kde.KWin.ScreenShot2.Error.NoAuthorized")
            || reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")) {
            m_kwinWorks = false;
            qWarning("KWin will not take screenshots for Wild OS Screenshot: %s", qPrintable(reply.errorMessage()));
        }
        ::close(fds[0]);
        return {};
    }
    const QVariantMap results = qdbus_cast<QVariantMap>(reply.arguments().constFirst());

    QFile pipeFile;
    if (!pipeFile.open(fds[0], QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fds[0]);
        return {};
    }
    const QByteArray data = pipeFile.readAll();
    const int width = results.value(QStringLiteral("width")).toInt();
    const int height = results.value(QStringLiteral("height")).toInt();
    const int stride = results.value(QStringLiteral("stride")).toInt();
    const auto format = QImage::Format(results.value(QStringLiteral("format")).toUInt());
    if (width <= 0 || height <= 0 || data.size() < qsizetype(stride) * height) {
        return {};
    }
    QImage image(reinterpret_cast<const uchar *>(data.constData()), width, height, stride, format);
    image = image.copy();
    image.setDevicePixelRatio(results.value(QStringLiteral("scale"), 1.0).toDouble());
    return image;
}

QHash<QString, QImage> Grabber::grabScreens()
{
    QHash<QString, QImage> images;
    const QVariantMap options{
        {QStringLiteral("include-cursor"), false},
        {QStringLiteral("native-resolution"), true},
    };
    for (QScreen *screen : QGuiApplication::screens()) {
        QImage image = callKWin(QStringLiteral("CaptureScreen"), {screen->name(), options});
        // On Wayland only KWin can see the screen; Qt's own grab would be
        // black, so without KWin there is no picture at all.
        if (image.isNull() && !QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
            image = screen->grabWindow(0).toImage();
            image.setDevicePixelRatio(screen->devicePixelRatio());
        }
        if (!image.isNull()) {
            images.insert(screen->name(), image);
        }
    }
    return images;
}

QImage Grabber::grabWindow(const QString &id)
{
    const QVariantMap options{
        {QStringLiteral("include-cursor"), false},
        {QStringLiteral("include-decoration"), true},
        {QStringLiteral("include-shadow"), false},
        {QStringLiteral("native-resolution"), true},
    };
    return callKWin(QStringLiteral("CaptureWindow"), {id, options});
}

void Grabber::requestWindows()
{
    auto *script = new QTemporaryFile(QStringLiteral("%1/wildos-screenshot-XXXXXX.js").arg(QDir::tempPath()), this);
    if (!script->open()) {
        delete script;
        Q_EMIT windowsArrived({});
        return;
    }
    script->write(windowScript);
    script->flush();

    const QString plugin = QStringLiteral("wildos-screenshot-windows");
    auto scripting = [](const QString &method, const QVariantList &args) {
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                                                              QStringLiteral("org.kde.kwin.Scripting"), method);
        message.setArguments(args);
        return QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    };
    scripting(QStringLiteral("unloadScript"), {plugin});
    const QDBusMessage loaded = scripting(QStringLiteral("loadScript"), {script->fileName(), plugin});
    if (loaded.type() != QDBusMessage::ReplyMessage || loaded.arguments().isEmpty()) {
        delete script;
        Q_EMIT windowsArrived({});
        return;
    }
    const int id = loaded.arguments().constFirst().toInt();
    // KWin 6 names script objects /Scripting/Script<n>; 5.x used /<n>.
    QDBusMessage run = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting/Script%1").arg(id),
                                                      QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run"));
    QDBusConnection::sessionBus().call(run, QDBus::Block, 2000);
    QTimer::singleShot(3000, this, [script, scripting, plugin] {
        scripting(QStringLiteral("unloadScript"), {plugin});
        delete script;
    });
}

WindowListReceiver::WindowListReceiver(Grabber *grabber)
    : QObject(grabber)
    , m_grabber(grabber)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/WindowList"), this, QDBusConnection::ExportScriptableSlots);
}

void WindowListReceiver::setWindows(const QString &json)
{
    Q_EMIT m_grabber->windowsArrived(QJsonDocument::fromJson(json.toUtf8()).array());
}
