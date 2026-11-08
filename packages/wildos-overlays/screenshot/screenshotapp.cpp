#include "screenshotapp.h"

#include "frozenimage.h"
#include "grabber.h"
#include "recorder.h"
#include "sharer.h"

#include <KConfigGroup>
#include <KNotification>
#include <KSharedConfig>
#include <KSystemClipboard>

#include <QBuffer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QStandardPaths>

namespace
{
ScreenshotApp *s_instance = nullptr;

QScreen *screenNamed(const QString &name)
{
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == name) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

KConfigGroup settings()
{
    return KSharedConfig::openConfig(QStringLiteral("wildosscreenshotrc"))->group(QStringLiteral("General"));
}
}

ScreenshotApp::ScreenshotApp(QObject *parent)
    : QObject(parent)
    , m_frozen(new FrozenImages)
    , m_grabber(new Grabber(this))
    , m_recorder(new Recorder(this))
    , m_sharer(new Sharer(this))
{
    s_instance = this;
    m_delay = settings().readEntry("Delay", 0);
    const QString mode = settings().readEntry("Mode", QStringLiteral("rectangle"));
    if (mode == QLatin1String("window") || mode == QLatin1String("screen") || mode == QLatin1String("freehand")) {
        m_mode = mode;
    }

    connect(m_grabber, &Grabber::windowsArrived, this, [this](const QJsonArray &windows) {
        m_windows = windows.toVariantList();
        Q_EMIT windowsChanged();
    });

    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this] {
        m_countdown -= 1;
        Q_EMIT countdownChanged();
        if (m_countdown <= 0) {
            m_countdownTimer.stop();
            // One frame for the countdown pill to go before the grab.
            QTimer::singleShot(150, this, &ScreenshotApp::freeze);
        }
    });

    connect(m_recorder, &Recorder::finished, this, &ScreenshotApp::recordingFinished);
    connect(m_recorder, &Recorder::failed, this, [this](const QString &message) {
        notifyFailure(QStringLiteral("Recording stopped"), message);
        quitWhenIdle();
    });
    connect(m_recorder, &Recorder::recordingChanged, this, &ScreenshotApp::quitWhenIdle);
    connect(m_sharer, &Sharer::finished, this, [this](const QString &message) {
        setMessage(message);
    });
    connect(m_sharer, &Sharer::failed, this, [this](const QString &message) {
        setMessage(message);
    });

    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(800);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] {
        if (m_selecting || m_countdown > 0 || m_recorder->recording() || m_openWindows > 0) {
            return;
        }
        for (const auto &notification : std::as_const(m_notifications)) {
            if (notification) {
                return;
            }
        }
        QGuiApplication::quit();
    });
}

ScreenshotApp::~ScreenshotApp()
{
    s_instance = nullptr;
}

ScreenshotApp *ScreenshotApp::instance()
{
    return s_instance;
}

QString ScreenshotApp::primaryScreen() const
{
    return QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->name() : QString();
}

bool ScreenshotApp::canRecord() const
{
    return m_recorder->available();
}

void ScreenshotApp::setMode(const QString &mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    settings().writeEntry("Mode", mode);
    settings().sync();
    Q_EMIT modeChanged();
}

void ScreenshotApp::setRecordMode(bool on)
{
    if (m_recordMode == on) {
        return;
    }
    m_recordMode = on;
    // Freehand cannot be recorded, so it gives way to a rectangle.
    if (on && m_mode == QLatin1String("freehand")) {
        m_mode = QStringLiteral("rectangle");
    }
    Q_EMIT modeChanged();
}

void ScreenshotApp::setDelay(int seconds)
{
    if (m_delay == seconds) {
        return;
    }
    m_delay = seconds;
    settings().writeEntry("Delay", seconds);
    settings().sync();
    Q_EMIT delayChanged();
}

void ScreenshotApp::setMessage(const QString &message)
{
    m_message = message;
    Q_EMIT messageChanged();
}

void ScreenshotApp::handle(const QStringList &arguments)
{
    const int markup = arguments.indexOf(QStringLiteral("--markup"));
    if (markup >= 0 && markup + 1 < arguments.size()) {
        openMarkup(QUrl::fromUserInput(arguments.at(markup + 1), QDir::currentPath(), QUrl::AssumeLocalFile));
        return;
    }
    if (m_recorder->recording()) {
        // The shortcut again while recording ends the recording, as the
        // Stop button does.
        m_recorder->stop();
        return;
    }
    if (m_selecting || m_countdown > 0) {
        return;
    }
    const int mode = arguments.indexOf(QStringLiteral("--mode"));
    if (mode >= 0 && mode + 1 < arguments.size()) {
        setMode(arguments.at(mode + 1));
    }
    setRecordMode(arguments.contains(QStringLiteral("--record")) && canRecord());
    begin();
}

void ScreenshotApp::begin()
{
    freeze();
}

void ScreenshotApp::freeze()
{
    const auto images = m_grabber->grabScreens();
    if (images.isEmpty()) {
        notifyFailure(QStringLiteral("No screenshot taken"),
                      QStringLiteral("The screen could not be read. Sign out and back in, then try again."));
        quitWhenIdle();
        return;
    }
    m_frozen->set(images);
    m_windows.clear();
    Q_EMIT windowsChanged();
    m_grabber->requestWindows();
    ++m_serial;
    m_selecting = true;
    Q_EMIT selectingChanged();
}

void ScreenshotApp::cancel()
{
    m_countdownTimer.stop();
    m_countdown = 0;
    Q_EMIT countdownChanged();
    m_selecting = false;
    Q_EMIT selectingChanged();
    quitWhenIdle();
}

void ScreenshotApp::startCountdown()
{
    if (m_delay <= 0) {
        return;
    }
    m_selecting = false;
    Q_EMIT selectingChanged();
    m_countdown = m_delay;
    Q_EMIT countdownChanged();
    m_countdownTimer.start();
}

QImage ScreenshotApp::crop(const QString &screenName, const QRectF &area) const
{
    const QImage image = m_frozen->image(screenName);
    QScreen *screen = screenNamed(screenName);
    if (image.isNull() || !screen) {
        return {};
    }
    const qreal scale = image.width() / qreal(screen->geometry().width());
    const QRect pixels = QRectF(area.x() * scale, area.y() * scale, area.width() * scale, area.height() * scale).toAlignedRect().intersected(image.rect());
    QImage result = image.copy(pixels);
    result.setDevicePixelRatio(image.devicePixelRatio());
    return result;
}

void ScreenshotApp::takeArea(const QString &screenName, const QRectF &area)
{
    m_selecting = false;
    Q_EMIT selectingChanged();
    if (area.width() < 1 || area.height() < 1) {
        quitWhenIdle();
        return;
    }
    if (m_recordMode) {
        QScreen *screen = screenNamed(screenName);
        const QRect global = area.translated(screen->geometry().topLeft()).toAlignedRect();
        m_recorder->recordArea(global, screen->devicePixelRatio());
        return;
    }
    finish(crop(screenName, area));
}

void ScreenshotApp::takeFreehand(const QString &screenName, const QVariantList &points)
{
    m_selecting = false;
    Q_EMIT selectingChanged();
    QPainterPath path;
    for (const QVariant &value : points) {
        const QPointF point = value.toPointF();
        if (path.elementCount() == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    const QRectF bounds = path.boundingRect();
    if (bounds.width() < 2 || bounds.height() < 2) {
        quitWhenIdle();
        return;
    }
    const QImage area = crop(screenName, bounds);
    if (area.isNull()) {
        quitWhenIdle();
        return;
    }
    // Outside the drawn shape is see-through, as Snipping Tool's freeform
    // snip leaves it.
    QImage shaped(area.size(), QImage::Format_ARGB32_Premultiplied);
    shaped.fill(Qt::transparent);
    {
        QPainter painter(&shaped);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal scale = area.width() / bounds.width();
        painter.scale(scale, scale);
        painter.translate(-bounds.topLeft());
        painter.setClipPath(path);
        painter.drawImage(bounds, area);
    }
    shaped.setDevicePixelRatio(area.devicePixelRatio());
    finish(shaped);
}

void ScreenshotApp::takeWindow(const QString &id, const QRectF &globalArea)
{
    m_selecting = false;
    Q_EMIT selectingChanged();
    if (m_recordMode) {
        m_recorder->recordWindow(id);
        return;
    }
    QImage image = m_grabber->grabWindow(id);
    if (image.isNull()) {
        // Without KWin's help, the window as it looked on screen.
        for (QScreen *screen : QGuiApplication::screens()) {
            const QRectF part = globalArea.intersected(screen->geometry());
            if (!part.isEmpty()) {
                image = crop(screen->name(), part.translated(-screen->geometry().topLeft()));
                break;
            }
        }
    }
    finish(image);
}

void ScreenshotApp::takeScreen(const QString &screenName)
{
    m_selecting = false;
    Q_EMIT selectingChanged();
    if (m_recordMode) {
        m_recorder->recordScreen(screenNamed(screenName));
        return;
    }
    finish(m_frozen->image(screenName));
}

QString ScreenshotApp::nextPath() const
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty()) {
        pictures = QDir::homePath() + QStringLiteral("/Pictures");
    }
    const QString folder = pictures + QStringLiteral("/Screenshots");
    QDir().mkpath(folder);
    const QString stem = folder + QStringLiteral("/Screenshot ") + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HHmmss"));
    QString path = stem + QStringLiteral(".png");
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = stem + QStringLiteral(" (%1).png").arg(n);
    }
    return path;
}

static void putOnClipboard(const QImage &image)
{
    auto *mime = new QMimeData;
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    mime->setData(QStringLiteral("image/png"), png);
    mime->setImageData(image);
    KSystemClipboard::instance()->setMimeData(mime, QClipboard::Clipboard);
}

void ScreenshotApp::finish(const QImage &image)
{
    if (image.isNull()) {
        notifyFailure(QStringLiteral("No screenshot taken"), QStringLiteral("The screen could not be read. Try again."));
        quitWhenIdle();
        return;
    }
    const QString path = nextPath();
    const bool saved = image.save(path, "PNG");
    putOnClipboard(image);
    const QUrl file = QUrl::fromLocalFile(path);

    auto *notification = new KNotification(QStringLiteral("captured"));
    notification->setComponentName(QStringLiteral("wildos-screenshot"));
    notification->setTitle(QStringLiteral("Screenshot copied"));
    notification->setText(saved ? QStringLiteral("Also saved to Pictures › Screenshots.")
                                : QStringLiteral("It could not be saved to Pictures › Screenshots, so paste it somewhere soon."));
    notification->setIconName(QStringLiteral("image-x-generic"));
    // Kept in the list after the toast goes, with its buttons still working
    // while this program waits for them.
    notification->setHint(QStringLiteral("resident"), true);
    notification->setHint(QStringLiteral("desktop-entry"), QStringLiteral("org.wildos.screenshot"));
    if (saved) {
        notification->setUrls({file});
        auto *open = notification->addDefaultAction(QStringLiteral("Open"));
        connect(open, &KNotificationAction::activated, this, [this, file, notification] {
            openFile(file);
            notification->close();
        });
        auto *markup = notification->addAction(QStringLiteral("Mark up"));
        connect(markup, &KNotificationAction::activated, this, [this, file, notification] {
            openMarkup(file);
            notification->close();
        });
        auto *share = notification->addAction(QStringLiteral("Share"));
        connect(share, &KNotificationAction::activated, this, [this, file, notification] {
            openShare(file);
            notification->close();
        });
    }
    connect(notification, &KNotification::closed, this, &ScreenshotApp::quitWhenIdle);
    m_notifications.append(notification);
    notification->sendEvent();
}

void ScreenshotApp::recordingFinished(const QString &path)
{
    const QUrl file = QUrl::fromLocalFile(path);
    auto *notification = new KNotification(QStringLiteral("recorded"));
    notification->setComponentName(QStringLiteral("wildos-screenshot"));
    notification->setTitle(QStringLiteral("Screen recording saved"));
    notification->setText(QStringLiteral("In Videos › Screen recordings."));
    notification->setIconName(QStringLiteral("video-x-generic"));
    notification->setHint(QStringLiteral("resident"), true);
    notification->setHint(QStringLiteral("desktop-entry"), QStringLiteral("org.wildos.screenshot"));
    notification->setUrls({file});
    auto *play = notification->addDefaultAction(QStringLiteral("Play"));
    connect(play, &KNotificationAction::activated, this, [this, file, notification] {
        openFile(file);
        notification->close();
    });
    auto *folder = notification->addAction(QStringLiteral("Show in folder"));
    connect(folder, &KNotificationAction::activated, this, [this, file, notification] {
        showInFolder(file);
        notification->close();
    });
    auto *share = notification->addAction(QStringLiteral("Share"));
    connect(share, &KNotificationAction::activated, this, [this, file, notification] {
        openShare(file);
        notification->close();
    });
    connect(notification, &KNotification::closed, this, &ScreenshotApp::quitWhenIdle);
    m_notifications.append(notification);
    notification->sendEvent();
}

void ScreenshotApp::notifyFailure(const QString &title, const QString &text)
{
    auto *notification = new KNotification(QStringLiteral("failed"));
    notification->setComponentName(QStringLiteral("wildos-screenshot"));
    notification->setTitle(title);
    notification->setText(text);
    notification->setIconName(QStringLiteral("dialog-warning"));
    notification->setHint(QStringLiteral("desktop-entry"), QStringLiteral("org.wildos.screenshot"));
    connect(notification, &KNotification::closed, this, &ScreenshotApp::quitWhenIdle);
    m_notifications.append(notification);
    notification->sendEvent();
}

void ScreenshotApp::copyImageFile(const QUrl &file)
{
    const QImage image(file.toLocalFile());
    if (!image.isNull()) {
        putOnClipboard(image);
    }
}

void ScreenshotApp::copyImage(const QVariant &image)
{
    const QImage picture = image.value<QImage>();
    if (!picture.isNull()) {
        putOnClipboard(picture);
    }
}

void ScreenshotApp::openMarkup(const QUrl &file)
{
    ++m_openWindows;
    Q_EMIT markupRequested(file);
}

void ScreenshotApp::openShare(const QUrl &file)
{
    ++m_openWindows;
    setMessage(QString());
    Q_EMIT shareRequested(file);
}

void ScreenshotApp::openFile(const QUrl &file)
{
    QDesktopServices::openUrl(file);
}

void ScreenshotApp::showInFolder(const QUrl &file)
{
    // Files answers the freedesktop file manager interface, so the folder
    // opens with the file selected.
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                                                          QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    message.setArguments({QStringList{file.toString()}, QString()});
    if (!QDBusConnection::sessionBus().send(message)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file.toLocalFile()).absolutePath()));
    }
}

void ScreenshotApp::windowClosed()
{
    m_openWindows = std::max(0, m_openWindows - 1);
    quitWhenIdle();
}

void ScreenshotApp::quitWhenIdle()
{
    m_idleTimer.start();
}
