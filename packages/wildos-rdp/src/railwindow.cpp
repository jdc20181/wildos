#include "railwindow.h"
#include "style.h"
#include "wayland.h"
#include "client.h"
#include "input.h"
#include "kwin.h"
#include "rail.h"

#include <KWindowEffects>

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>

#include <freerdp/input.h>
#include <freerdp/rail.h>
#include <freerdp/window.h>

#include <cmath>
#include <cstring>

namespace
{
constexpr quint32 stateMinimized = 0x02;
constexpr quint32 stateMaximized = 0x03;
constexpr quint32 stateHidden = 0x00;
constexpr qreal cornerRadius = 12;
// Once the app has drawn, how long it may take to come to the loading
// window's size before it is shown at the size Windows gave it.
constexpr int handoffResizeMs = 400;
// An app window that has drawn nothing by then is shown anyway: Windows
// says it is open, so the loading window has nothing left to wait for.
constexpr int handoffBlankMs = 4000;
// How long the loading window takes to fade into the app (board 9.11's
// closing sequence, wlcomplete to wlapp).
constexpr int finishMs = 1760;

QRect deviceRect(const QRect &logical, qreal dpr, const QSize &bounds)
{
    const int x0 = int(std::floor(logical.left() * dpr));
    const int y0 = int(std::floor(logical.top() * dpr));
    const int x1 = int(std::ceil((logical.left() + logical.width()) * dpr));
    const int y1 = int(std::ceil((logical.top() + logical.height()) * dpr));
    return QRect(x0, y0, x1 - x0, y1 - y0).intersected(QRect(QPoint(0, 0), bounds));
}

QRect logicalRect(const QRect &device, qreal dpr)
{
    const int x0 = int(std::floor(device.left() / dpr));
    const int y0 = int(std::floor(device.top() / dpr));
    const int x1 = int(std::ceil((device.left() + device.width()) / dpr));
    const int y1 = int(std::ceil((device.top() + device.height()) / dpr));
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

QPainterPath roundedPath(const QSizeF &size)
{
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), size).adjusted(0.5, 0.5, -0.5, -0.5), cornerRadius, cornerRadius);
    return path;
}

Qt::Edges edgesFor(quint16 type)
{
    switch (type) {
    case RAIL_WMSZ_LEFT: return Qt::LeftEdge;
    case RAIL_WMSZ_RIGHT: return Qt::RightEdge;
    case RAIL_WMSZ_TOP: return Qt::TopEdge;
    case RAIL_WMSZ_TOPLEFT: return Qt::TopEdge | Qt::LeftEdge;
    case RAIL_WMSZ_TOPRIGHT: return Qt::TopEdge | Qt::RightEdge;
    case RAIL_WMSZ_BOTTOM: return Qt::BottomEdge;
    case RAIL_WMSZ_BOTTOMLEFT: return Qt::BottomEdge | Qt::LeftEdge;
    case RAIL_WMSZ_BOTTOMRIGHT: return Qt::BottomEdge | Qt::RightEdge;
    default: return {};
    }
}
}

RailWindow::RailWindow(Rail *rail, Client *client, const QString &appId)
    : m_rail(rail)
    , m_client(client)
    , m_appId(appId)
{
    setSurfaceType(QSurface::RasterSurface);
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    setFormat(format);
    setFlags(Qt::Window | Qt::FramelessWindowHint);
    m_store = std::make_unique<QBackingStore>(this);
    setCursor(rail->cursor());
    m_handoffTimer.setSingleShot(true);
    connect(&m_handoffTimer, &QTimer::timeout, this, [this] {
        if (m_handoffResizing && !m_hasSurface && !m_primary) {
            // Nothing drawn yet: whatever comes first is shown, at any size.
            m_handoffResizing = false;
            m_handoffTimer.start(handoffBlankMs);
            return;
        }
        appReady();
    });
}

RailWindow::~RailWindow()
{
    releaseKeys();
}

double RailWindow::scale() const
{
    return m_rail->scale();
}

QSize RailWindow::logicalSize(const QSize &remote) const
{
    return QSize(std::max(1, int(std::lround(remote.width() / scale()))), std::max(1, int(std::lround(remote.height() / scale()))));
}

bool RailWindow::remoteVisible() const
{
    return m_id != 0 && m_showState != stateHidden && m_frame.width() > 0 && m_frame.height() > 0;
}

void RailWindow::startLoading(const QString &title, const QIcon &icon, const QSize &size)
{
    m_loading = true;
    setTitle(title);
    resize(size);
    m_view.start(title, icon, Style::read().reducedMotion);
    KWindowEffects::enableBlurBehind(this, true, QRegion(roundedPath(size).toFillPolygon().toPolygon()));
}

void RailWindow::stopLoading()
{
    if (!m_loading) {
        return;
    }
    m_loading = false;
    m_handoffTimer.stop();
    KWindowEffects::enableBlurBehind(this, false);
    const QString title = property("remoteTitle").toString();
    if (!title.isEmpty()) {
        setTitle(title);
    }
    if (m_frame.isValid() && logicalSize(m_frame.size()) != intendedSize()) {
        resizeAroundCentre(logicalSize(m_frame.size()));
    }
    m_damage = QRect(QPoint(0, 0), m_hasSurface ? m_surfaceSize : m_frame.size());
    m_fullRepaint = true;
    requestUpdate();
}

QSize RailWindow::intendedSize() const
{
    return m_targetSize.isValid() ? m_targetSize : size();
}

void RailWindow::resizeAroundCentre(const QSize &to)
{
    if (to == intendedSize()) {
        return;
    }
    m_targetSize = to;
    if (!KWinPlace::resizeAroundCentre(this, m_appId, to)) {
        resize(to);
        return;
    }
    QTimer::singleShot(400, this, [this, to] {
        if (m_targetSize == to && size() != to) {
            // KWin did not do it: the size matters more than the place.
            resize(to);
        }
    });
}

void RailWindow::setLoadingStage(LoadingView::Stage stage)
{
    m_view.setStage(stage);
}

void RailWindow::setLoadingEnded()
{
    m_view.setEnded();
    m_fullRepaint = true;
    requestUpdate();
}

void RailWindow::setLoadingReason(const QString &reason, int percent, const QString &progress)
{
    m_view.setReason(reason, percent, progress);
    m_fullRepaint = true;
}

void RailWindow::beginHandoff()
{
    m_view.setStage(LoadingView::Stage::AlmostReady);
    m_view.arrive();
    // Nothing moves or resizes once the app shows. A resizable app is asked
    // to take this window's size; one Windows will not resize (a fixed
    // dialog such as Character Map) gives this window its own size now,
    // before its picture comes. The app is shown as soon as it has drawn,
    // whether or not it took the size: the handoff never waits on the app
    // agreeing to it.
    const QSize wanted(int(std::lround(width() * scale())), int(std::lround(height() * scale())));
    m_handoffSize = m_frame.size();
    m_handoffResizing = false;
    if ((m_style & WS_THICKFRAME) && m_frame.isValid() && m_frame.size() != wanted && !(m_style & WS_MAXIMIZE)) {
        m_handoffSize = wanted;
        m_handoffResizing = true;
        m_client->moveWindow(m_id, QRect(m_frame.topLeft(), wanted));
    } else if (m_frame.isValid() && !(m_style & WS_MAXIMIZE) && logicalSize(m_frame.size()) != intendedSize()) {
        resizeAroundCentre(logicalSize(m_frame.size()));
        m_fullRepaint = true;
        requestUpdate();
    }
    m_handoffTimer.start(m_handoffResizing ? handoffResizeMs * 3 : handoffBlankMs);
    if (m_hasSurface || m_primary) {
        handoffContent(m_hasSurface ? m_surfaceSize : m_frame.size());
    }
}

void RailWindow::handoffContent(const QSize &drawn)
{
    if (!m_loading || !m_id || m_view.finishing() || !m_handoffSize.isValid()) {
        return;
    }
    if (!m_handoffResizing || drawn == m_handoffSize) {
        appReady();
        return;
    }
    // Drawn, but not at the size asked for yet: it is given a moment.
    if (!m_handoffTimer.isActive() || m_handoffTimer.remainingTime() > handoffResizeMs) {
        m_handoffTimer.start(handoffResizeMs);
    }
}

void RailWindow::appReady()
{
    if (!m_loading || m_view.finishing()) {
        return;
    }
    m_handoffTimer.stop();
    if (m_frame.isValid() && logicalSize(m_frame.size()) != intendedSize()) {
        // Windows kept its own size (the app did not take this window's):
        // the window takes Windows', once, as the app comes in.
        resizeAroundCentre(logicalSize(m_frame.size()));
    }
    m_view.finish();
    m_fullRepaint = true;
    requestUpdate();
    // The fade is paced by the compositor's frames; if one never comes (a
    // frame lost around a resize), the window would be left half faded
    // into the loading window. It is finished by the clock as well.
    for (int at : {finishMs + 100, finishMs + 1000}) {
        QTimer::singleShot(at, this, [this] {
            if (m_loading && m_view.done()) {
                render();
            }
        });
    }
}

void RailWindow::adopt(quint32 id)
{
    m_id = id;
    // Windows sends a new window's picture straight away; if it has not
    // come (a window Windows had drawn before this connection), it is asked
    // for again.
    for (int delay : {800, 2500}) {
        QTimer::singleShot(delay, this, [this] {
            if (m_loading && !m_hasSurface && !m_primary && m_frame.isValid()) {
                m_client->refresh(m_frame);
            }
        });
    }
}

void RailWindow::unadopt()
{
    m_id = 0;
    m_hasSurface = false;
    m_primary = false;
    m_hasContent = false;
    m_surfaceId = 0;
    m_surfaceSize = QSize();
    m_frame = QRect();
    m_damage = QRegion();
    m_handoffTimer.stop();
    m_handoffSize = QSize();
    m_handoffResizing = false;
    m_view.unarrive();
    m_fullRepaint = true;
    requestUpdate();
}

void RailWindow::setRemoteState(quint32 id, quint32 style, quint32 exStyle, bool popup)
{
    m_id = id;
    m_style = style;
    m_exStyle = exStyle;
    m_layered = (exStyle & WS_EX_LAYERED) && popup;
    if (popup != m_popup && !handle()) {
        m_popup = popup;
        // Menus and tooltips are popups of the window they belong to; they
        // never take the keyboard from it, which Windows keeps routing.
        setFlags(popup ? (Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus) : (Qt::Window | Qt::FramelessWindowHint));
    }
}

void RailWindow::setFrame(const QRect &frame)
{
    const bool resized = frame.size() != m_frame.size();
    m_frame = frame;
    if (resized && !m_loading && !(windowStates() & Qt::WindowMaximized)) {
        resize(logicalSize(frame.size()));
    }
}

void RailWindow::setShowState(quint32 state)
{
    m_showState = state;
}

void RailWindow::setRemoteTitle(const QString &title)
{
    if (!m_loading) {
        setTitle(title);
    } else {
        // Kept for when the app takes over.
        setProperty("remoteTitle", title);
    }
}

void RailWindow::setIcon(const QImage &image, bool big)
{
    if (image.isNull()) {
        return;
    }
    if (!big && !m_remoteIcon.isNull()) {
        return;
    }
    m_remoteIcon = image;
    QWindow::setIcon(QIcon(QPixmap::fromImage(image)));
    if (isVisible()) {
        sendIcon();
    }
}

void RailWindow::sendIcon()
{
    // Qt 6.8 does not pass a window's icon on to the compositor; the icon
    // Windows sent (or the app's, while loading) goes over the protocol
    // for it, so the taskbar shows it even for an app with no entry.
    if (m_popup) {
        return;
    }
    QImage image = m_remoteIcon;
    if (image.isNull() && !icon().isNull()) {
        image = icon().pixmap(QSize(64, 64)).toImage();
    }
    if (!image.isNull()) {
        m_iconSent = WaylandExtras::instance().setIcon(this, image);
    }
}

void RailWindow::setAppId(const QString &appId)
{
    if (appId == m_appId) {
        return;
    }
    m_appId = appId;
    if (!isVisible() || m_popup || WaylandExtras::instance().setAppId(this, appId)) {
        return;
    }
    // Without the window's own protocol object the app id is only read
    // when the window is shown, so it is shown again to carry the new one.
    {
        const QString saved = QGuiApplication::desktopFileName();
        QGuiApplication::setDesktopFileName(appId);
        hide();
        show();
        QGuiApplication::setDesktopFileName(saved);
    }
}

void RailWindow::setMinMax(const QSize &minTrack, const QSize &maxTrack)
{
    if (minTrack.isValid() && minTrack.width() > 0) {
        setMinimumSize(logicalSize(minTrack));
    }
    if (maxTrack.isValid() && maxTrack.width() > 0 && maxTrack.width() < 30000) {
        setMaximumSize(logicalSize(maxTrack));
    }
}

void RailWindow::surfaceDamaged(quint16 surfaceId, const QRegion &damage, const QSize &size)
{
    if (!m_hasSurface || surfaceId != m_surfaceId || size != m_surfaceSize) {
        m_surfaceId = surfaceId;
        m_surfaceSize = size;
        m_hasSurface = true;
        m_primary = false;
        m_damage = QRect(QPoint(0, 0), size);
    } else {
        m_damage += damage;
    }
    handoffContent(size);
    if (!m_updateQueued) {
        m_updateQueued = true;
        requestUpdate();
    }
}

void RailWindow::primaryDamaged(const QRegion &desktopDamage)
{
    if (m_hasSurface) {
        return;
    }
    const QRegion mine = desktopDamage.intersected(m_frame).translated(-m_frame.topLeft());
    if (mine.isEmpty()) {
        return;
    }
    m_primary = true;
    m_damage += mine;
    handoffContent(m_frame.size());
    if (!m_updateQueued) {
        m_updateQueued = true;
        requestUpdate();
    }
}

bool RailWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Hide:
        // A window shown again is a new toplevel to the compositor.
        m_iconSent = false;
        break;
    case QEvent::UpdateRequest:
        m_updateQueued = false;
        render();
        return true;
    case QEvent::Close:
        event->ignore();
        if (m_id) {
            // The app's window, or the loading window that has taken it and
            // is handing over: either way the app is what is being closed.
            if (!m_loading && (m_keysDown.contains(0x38) || m_keysDown.contains(0x38 | 0x100))) {
                // Alt+F4 is the compositor's: Windows saw Alt go down and will
                // see it come up with nothing between, which opens the app's
                // menu or its key tips. A Ctrl tap in between stops that.
                m_client->sendKey(true, false, 0x1D);
                m_client->sendKey(false, false, 0x1D);
            }
            m_rail->closeRequested(this);
        } else if (m_loading) {
            m_rail->cancel(property("token").toUInt(), QStringLiteral("cancelled"));
        }
        return true;
    case QEvent::Enter:
        if (m_moveSize) {
            finishMoveSize(static_cast<QEnterEvent *>(event)->position());
        }
        break;
    case QEvent::WindowStateChange:
        if (m_id && !m_loading && !m_popup) {
            const bool maximized = windowStates() & Qt::WindowMaximized;
            if (maximized && m_showState != stateMaximized) {
                m_client->sysCommand(m_id, SC_MAXIMIZE);
            } else if (!maximized && m_showState == stateMaximized) {
                m_client->sysCommand(m_id, SC_RESTORE);
            }
        }
        break;
    default:
        break;
    }
    return QWindow::event(event);
}

void RailWindow::exposeEvent(QExposeEvent *)
{
    if (!isExposed()) {
        return;
    }
    m_loadingPainted = false;
    if (!m_iconSent) {
        sendIcon();
    }
    if (m_loading) {
        m_rail->loadingShown(property("token").toUInt());
    }
    m_damage = QRect(QPoint(0, 0), m_hasSurface ? m_surfaceSize : m_frame.size());
    render();
}

void RailWindow::resizeEvent(QResizeEvent *event)
{
    m_store->resize(event->size());
    m_loadingPainted = false;
    if (event->size() == m_targetSize) {
        m_targetSize = QSize();
    }
    if (m_loading) {
        KWindowEffects::enableBlurBehind(this, true, QRegion(roundedPath(event->size()).toFillPolygon().toPolygon()));
    }
    if (m_hasSurface) {
        m_damage = QRect(QPoint(0, 0), m_surfaceSize);
    }
    if (!m_loading && m_id && !m_popup && !m_moveSize) {
        // The compositor sized the window itself (snapped to half the
        // screen, maximised): Windows follows.
        const QSize remote(int(std::lround(event->size().width() * scale())), int(std::lround(event->size().height() * scale())));
        const bool maximized = windowStates() & Qt::WindowMaximized;
        if (maximized) {
            m_client->setWorkArea(QRect(QPoint(0, 0), remote));
        } else if (logicalSize(m_frame.size()) != event->size() && m_frame.isValid()) {
            m_client->moveWindow(m_id, QRect(m_frame.topLeft(), remote));
        }
    }
}

void RailWindow::render()
{
    if (!isExposed()) {
        return;
    }
    if (m_loading) {
        if (m_view.done()) {
            stopLoading();
        } else {
            renderLoading();
            return;
        }
    }
    renderRemote();
}

void RailWindow::renderLoading()
{
    const QRect all(QPoint(0, 0), size());
    // While the light runs its laps only the mark and the line under it
    // change; everything else is painted when it does.
    const bool slow = m_view.slow();
    QRegion region = all;
    bool steady = false;
    if (!m_fullRepaint && slow == m_wasSlow && !m_view.finishing() && m_loadingPainted && m_view.settled()) {
        region = m_view.steadyArea(QSizeF(size())).toAlignedRect();
        steady = true;
    }
    m_fullRepaint = false;
    m_wasSlow = slow;
    m_store->beginPaint(region);
    {
        QPainter p(m_store->paintDevice());
        p.setClipRegion(region);
        const qreal dpr = devicePixelRatio();
        if (steady && m_loadingBackground.size() == (QSizeF(size()) * dpr).toSize()) {
            // The frame and title bar do not change between frames: kept from
            // the last full paint, only the W and its words are drawn.
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.drawImage(QPointF(0, 0), m_loadingBackground);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            m_view.paint(p, QSizeF(size()), false, true);
        } else {
            paintLoading(p);
            if (!m_view.finishing() && m_view.settled()) {
                m_loadingBackground = QImage((QSizeF(size()) * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
                m_loadingBackground.setDevicePixelRatio(dpr);
                m_loadingBackground.fill(Qt::transparent);
                QPainter bg(&m_loadingBackground);
                m_view.paint(bg, QSizeF(size()), true, false);
            }
        }
        if (m_view.finishing() && (m_hasSurface || m_primary)) {
            // wlapp: the app fades in where it will stay.
            const QImage app = snapshot();
            p.setOpacity(m_view.appOpacity());
            p.drawImage(QRectF(QPointF(0, 0), QSizeF(app.size()) / scale()), app);
        }
    }
    m_store->endPaint();
    m_store->flush(region);
    m_loadingPainted = true;
    if (m_view.animating()) {
        // Paced by the compositor's frames, and stopped while hidden.
        requestUpdate();
    }
}

void RailWindow::renderRemote()
{
    if (m_damage.isEmpty()) {
        return;
    }
    const qreal dpr = devicePixelRatio();
    const QSize logical = size();
    QRegion logicalDamage;
    for (const QRect &r : m_damage) {
        logicalDamage += logicalRect(r, dpr).intersected(QRect(QPoint(0, 0), logical));
    }
    m_damage = QRegion();
    if (logicalDamage.isEmpty()) {
        return;
    }
    m_store->beginPaint(logicalDamage);
    auto *image = dynamic_cast<QImage *>(m_store->paintDevice());
    const bool exact = image && std::abs(dpr - scale()) < 0.01;
    bool opaque = !m_layered && !m_alpha;
    auto copy = [&](const uchar *data, int stride, QSize size) {
        if (exact) {
            const QSize bounds = image->size().boundedTo(size);
            for (const QRect &l : logicalDamage) {
                const QRect d = deviceRect(l, dpr, bounds);
                for (int y = d.top(); y <= d.bottom(); ++y) {
                    auto *dst = reinterpret_cast<quint32 *>(image->scanLine(y)) + d.left();
                    const auto *src = reinterpret_cast<const quint32 *>(data + qsizetype(y) * stride) + d.left();
                    if (opaque) {
                        for (int x = 0; x < d.width(); ++x) {
                            dst[x] = src[x] | 0xff000000u;
                        }
                    } else {
                        std::memcpy(dst, src, size_t(d.width()) * 4);
                    }
                }
            }
        } else {
            // This screen's scale is not the one Windows draws at (the window
            // is on another screen): scaled, which costs some sharpness.
            const QImage source(data, size.width(), size.height(), stride, opaque ? QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied);
            QPainter p(m_store->paintDevice());
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.setClipRegion(logicalDamage);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.drawImage(QRectF(0, 0, size.width() / scale(), size.height() / scale()), source);
        }
    };
    bool ok = false;
    if (m_hasSurface && m_popup && !m_alphaChecked) {
        // A popup Windows draws with real transparency (rounded menus, key
        // tips) is drawn with it here; one drawn by GDI leaves the alpha
        // byte empty everywhere and is opaque.
        m_client->withSurface(m_surfaceId, [this](const uchar *data, int stride, QSize size) {
            bool full = false;
            bool partial = false;
            for (int y = 0; y < size.height() && !(full && partial); ++y) {
                const auto *row = reinterpret_cast<const quint32 *>(data + qsizetype(y) * stride);
                for (int x = 0; x < size.width(); ++x) {
                    const quint32 a = row[x] >> 24;
                    full |= a == 0xff;
                    partial |= a != 0xff;
                }
            }
            m_alpha = full && partial;
            m_alphaChecked = true;
        });
        opaque = !m_layered && !m_alpha;
    }
    if (m_hasSurface) {
        ok = m_client->withSurface(m_surfaceId, [&](const uchar *data, int stride, QSize size) { copy(data, stride, size); });
    } else if (m_primary) {
        ok = m_client->withPrimary([&](const uchar *data, int stride, QSize desktop) {
            const QRect area = m_frame.intersected(QRect(QPoint(0, 0), desktop));
            if (area.isEmpty()) {
                return;
            }
            const uchar *origin = data + qsizetype(area.top()) * stride + qsizetype(area.left()) * 4;
            copy(origin, stride, area.size());
        });
    }
    m_store->endPaint();
    if (ok) {
        m_store->flush(logicalDamage);
        if (!m_hasContent) {
            m_hasContent = true;
            m_rail->windowDrawn(this);
        }
    }
}

QPoint RailWindow::toRemote(const QPointF &local) const
{
    return QPoint(m_frame.x() + int(std::lround(local.x() * scale())), m_frame.y() + int(std::lround(local.y() * scale())));
}

void RailWindow::sendButton(QMouseEvent *event, bool down)
{
    rdpinput::button(m_client, event->button(), down, toRemote(event->position()));
}

void RailWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_loading) {
        loadingPress(event);
        return;
    }
    if (!m_id) {
        return;
    }
    if (!m_popup) {
        m_rail->windowActivated(this, true);
    }
    if (event->button() == Qt::LeftButton) {
        m_pressRemote = toRemote(event->position());
    }
    sendButton(event, true);
}

void RailWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_id || m_loading) {
        return;
    }
    sendButton(event, false);
}

void RailWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_loading) {
        const LoadingView::Hit hit = m_view.hitTest(event->position(), QSizeF(size()));
        const LoadingView::Hit hover = hit == LoadingView::Hit::Drag ? LoadingView::Hit::None : hit;
        if (hover != m_view.hover()) {
            m_view.setHover(hover);
            m_fullRepaint = true;
            requestUpdate();
        }
        return;
    }
    if (!m_id) {
        return;
    }
    if (m_moveSize) {
        if (event->buttons() != Qt::NoButton) {
            return;
        }
        finishMoveSize(event->position());
    }
    rdpinput::move(m_client, toRemote(event->position()));
}

void RailWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_loading) {
        if (m_view.hitTest(event->position(), QSizeF(size())) == LoadingView::Hit::Drag) {
            windowStates() & Qt::WindowMaximized ? showNormal() : showMaximized();
        }
        return;
    }
    // Qt sends the second press of a double click as well; Windows works out
    // double clicks from those itself.
}

void RailWindow::loadingPress(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const quint32 token = property("token").toUInt();
    switch (m_view.hitTest(event->position(), QSizeF(size()))) {
    case LoadingView::Hit::Drag: startSystemMove(); break;
    case LoadingView::Hit::Minimise: showMinimized(); break;
    case LoadingView::Hit::Maximise: windowStates() & Qt::WindowMaximized ? showNormal() : showMaximized(); break;
    case LoadingView::Hit::Close:
    case LoadingView::Hit::Cancel: m_rail->cancel(token, QStringLiteral("cancelled")); break;
    case LoadingView::Hit::Details: m_rail->detailsRequested(token); break;
    case LoadingView::Hit::Retry: m_rail->retryRequested(token); break;
    case LoadingView::Hit::None: break;
    }
}

void RailWindow::wheelEvent(QWheelEvent *event)
{
    if (!m_id || m_loading) {
        return;
    }
    rdpinput::wheel(m_client, event->angleDelta(), toRemote(event->position()), m_wheelX, m_wheelY);
}

void RailWindow::keyPressEvent(QKeyEvent *event)
{
    if (!m_id || m_loading) {
        return;
    }
    rdpinput::key(m_client, event, true, m_keysDown);
}

void RailWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_id || m_loading) {
        return;
    }
    rdpinput::key(m_client, event, false, m_keysDown);
}

void RailWindow::releaseKeys()
{
    // Keys held when the window lost the keyboard would otherwise stay down
    // in Windows: Alt from Alt+Tab is the usual one.
    rdpinput::releaseAll(m_client, m_keysDown);
}

void RailWindow::focusInEvent(QFocusEvent *)
{
    if (m_id && !m_loading && !m_popup) {
        m_rail->windowActivated(this, true);
    }
}

void RailWindow::focusOutEvent(QFocusEvent *)
{
    releaseKeys();
    if (m_id && !m_loading && !m_popup) {
        m_rail->windowActivated(this, false);
    }
}

void RailWindow::beginLocalMoveSize(quint16 type)
{
    bool started = false;
    if (type == RAIL_WMSZ_MOVE) {
        started = startSystemMove();
    } else if (const Qt::Edges edges = edgesFor(type)) {
        started = startSystemResize(edges);
    }
    if (started) {
        m_moveSize = true;
        m_moveSizeType = type;
        m_moveSizeStart = m_frame;
    }
}

void RailWindow::endLocalMoveSize()
{
    m_moveSize = false;
}

void RailWindow::finishMoveSize(const QPointF &)
{
    if (!m_moveSize) {
        return;
    }
    m_moveSize = false;
    // The compositor kept the button's release, so Windows is still in its
    // move loop: it gets the release where the press was, which leaves its
    // window where it is, and then the size the window was given here.
    m_client->sendMouse(PTR_FLAGS_BUTTON1, m_pressRemote.x(), m_pressRemote.y());
    if (m_moveSizeType == RAIL_WMSZ_MOVE) {
        return;
    }
    const QSize remote(int(std::lround(width() * scale())), int(std::lround(height() * scale())));
    const Qt::Edges edges = edgesFor(m_moveSizeType);
    QRect next(m_moveSizeStart.topLeft(), remote);
    if (edges & Qt::LeftEdge) {
        next.moveLeft(m_moveSizeStart.left() + m_moveSizeStart.width() - remote.width());
    }
    if (edges & Qt::TopEdge) {
        next.moveTop(m_moveSizeStart.top() + m_moveSizeStart.height() - remote.height());
    }
    if (next != m_frame) {
        m_client->moveWindow(m_id, next);
    }
}

QImage RailWindow::snapshot() const
{
    QImage out;
    auto copy = [&out](const uchar *data, int stride, QSize size) {
        out = QImage(data, size.width(), size.height(), stride, QImage::Format_RGB32).copy();
    };
    if (m_hasSurface) {
        m_client->withSurface(m_surfaceId, copy);
    } else if (m_primary) {
        m_client->withPrimary([&](const uchar *data, int stride, QSize desktop) {
            const QRect area = m_frame.intersected(QRect(QPoint(0, 0), desktop));
            if (!area.isEmpty()) {
                copy(data + qsizetype(area.top()) * stride + qsizetype(area.left()) * 4, stride, area.size());
            }
        });
    }
    return out;
}

void RailWindow::paintLoading(QPainter &p) const
{
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(QPoint(0, 0), size()), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    m_view.paint(p, QSizeF(size()));
}

QImage RailWindow::loadingSnapshot(qreal dpr) const
{
    QImage image((QSizeF(size()) * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    paintLoading(p);
    return image;
}
