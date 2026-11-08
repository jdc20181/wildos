#include "windowframe.h"

#include <KWindowEffects>
#include <KWindowShadow>

#include <QEvent>
#include <QImage>
#include <QRegion>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace
{
// A CSS blur of 80px is a Gaussian with a sigma of 40, which has faded to
// nothing two sigmas out; the offset is the token's 30px drop.
constexpr int shadowOffset = 30;
constexpr double shadowSigma = 40.0;
constexpr int shadowReach = 80;

double coverage(double t, double from, double to)
{
    const double k = shadowSigma * std::sqrt(2.0);
    return 0.5 * (std::erf((t - from) / k) - std::erf((t - to) / k));
}

bool insideRounded(double x, double y, const QRectF &rect, double radius)
{
    if (!rect.contains(x, y)) {
        return false;
    }
    const double cx = std::clamp(x, rect.left() + radius, rect.right() - radius);
    const double cy = std::clamp(y, rect.top() + radius, rect.bottom() - radius);
    const double dx = x - cx;
    const double dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

QRegion roundedRegion(const QSize &size, int radius)
{
    if (radius <= 0) {
        return QRegion(QRect(QPoint(0, 0), size));
    }
    QRegion region(QRect(0, radius, size.width(), size.height() - 2 * radius));
    for (int row = 0; row < radius; ++row) {
        const double dy = radius - row - 0.5;
        const int inset = int(std::ceil(radius - std::sqrt(std::max(0.0, double(radius * radius) - dy * dy))));
        region += QRect(inset, row, size.width() - 2 * inset, 1);
        region += QRect(inset, size.height() - 1 - row, size.width() - 2 * inset, 1);
    }
    return region;
}

KWindowShadowTile::Ptr tile(const QImage &image, const QRect &area)
{
    auto t = KWindowShadowTile::Ptr::create();
    t->setImage(image.copy(area));
    t->create();
    return t;
}
}

WindowFrame::WindowFrame(QObject *parent)
    : QObject(parent)
{
    m_blurAvailable = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

WindowFrame::~WindowFrame() = default;

void WindowFrame::setWindow(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
        m_window->removeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
        connect(m_window, &QWindow::visibleChanged, this, &WindowFrame::schedule);
        connect(m_window, &QWindow::widthChanged, this, &WindowFrame::schedule);
        connect(m_window, &QWindow::heightChanged, this, &WindowFrame::schedule);
        connect(m_window, &QWindow::windowStateChanged, this, &WindowFrame::schedule);
    }
    Q_EMIT changed();
    schedule();
}

void WindowFrame::setRadius(int radius)
{
    if (m_radius != radius) {
        m_radius = radius;
        dropShadow();
        Q_EMIT changed();
        schedule();
    }
}

void WindowFrame::setShadow(bool on)
{
    if (m_shadowOn != on) {
        m_shadowOn = on;
        Q_EMIT changed();
        schedule();
    }
}

void WindowFrame::setShadowColor(const QColor &colour)
{
    if (m_shadowColour != colour) {
        m_shadowColour = colour;
        dropShadow();
        Q_EMIT changed();
        schedule();
    }
}

void WindowFrame::setBlur(bool on)
{
    if (m_blurOn != on) {
        m_blurOn = on;
        m_blurSize = {};
        Q_EMIT changed();
        schedule();
    }
}

void WindowFrame::recheck()
{
    const bool available = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
    if (available != m_blurAvailable) {
        m_blurAvailable = available;
        m_blurSize = {};
        Q_EMIT blurAvailableChanged();
        schedule();
    }
}

void WindowFrame::toggleMaximised()
{
    if (!m_window) {
        return;
    }
    if (m_window->windowStates() & Qt::WindowMaximized) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
}

bool WindowFrame::eventFilter(QObject *watched, QEvent *event)
{
    // Shadows and blur belong to the surface; a recreated surface (hidden
    // and shown again, moved to another screen) starts without them.
    if (watched == m_window && event->type() == QEvent::PlatformSurface) {
        dropShadow();
        m_blurSize = {};
        schedule();
    }
    return QObject::eventFilter(watched, event);
}

void WindowFrame::dropShadow()
{
    if (m_shadow) {
        m_shadow->destroy();
        delete m_shadow;
        m_shadow = nullptr;
    }
}

// Resizing sends a burst of size changes; one update per event loop pass is
// plenty for the compositor.
void WindowFrame::schedule()
{
    if (m_pending) {
        return;
    }
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_pending = false;
        apply();
    });
}

void WindowFrame::apply()
{
    if (!m_window) {
        return;
    }
    const bool full = m_window->windowStates() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    if (full != m_maximised) {
        m_maximised = full;
        Q_EMIT maximisedChanged();
    }
    if (!m_window->isVisible() || !m_window->handle()) {
        return;
    }
    applyShadow(full);
    applyBlur(full);
}

void WindowFrame::applyShadow(bool full)
{
    if (!m_shadowOn || full) {
        dropShadow();
        return;
    }
    if (m_shadow && m_shadow->window() == m_window && m_shadow->isCreated()) {
        return;
    }

    const int r = m_radius;
    const int padLeft = shadowReach;
    const int padRight = shadowReach;
    const int padTop = shadowReach - shadowOffset;
    const int padBottom = shadowReach + shadowOffset;
    // A reference window wide enough that its far edges add nothing to the
    // pieces cut from the near ones.
    const int side = 2 * (shadowReach + r) + 1;
    const int width = padLeft + side + padRight;
    const int height = padTop + side + padBottom;
    const QRectF windowRect(padLeft, padTop, side, side);
    const double peak = m_shadowColour.alphaF();

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const double cy = y + 0.5;
        const double fy = coverage(cy, windowRect.top() + shadowOffset, windowRect.bottom() + shadowOffset);
        for (int x = 0; x < width; ++x) {
            const double cx = x + 0.5;
            // Nothing under the window itself: the glass is translucent and
            // a shadow beneath it would muddy it.
            if (insideRounded(cx, cy, windowRect, r)) {
                line[x] = 0;
                continue;
            }
            const double a = peak * fy * coverage(cx, windowRect.left(), windowRect.right());
            const int alpha = std::clamp(int(std::lround(a * 255.0)), 0, 255);
            line[x] = qPremultiply(qRgba(m_shadowColour.red(), m_shadowColour.green(), m_shadowColour.blue(), alpha));
        }
    }

    const int midX = padLeft + side / 2;
    const int midY = padTop + side / 2;
    const int right = padLeft + side;
    const int bottom = padTop + side;

    if (!m_shadow) {
        m_shadow = new KWindowShadow(this);
    } else {
        m_shadow->destroy();
    }
    m_shadow->setTopLeftTile(tile(image, QRect(0, 0, padLeft + r, padTop + r)));
    m_shadow->setTopTile(tile(image, QRect(midX, 0, 1, padTop)));
    m_shadow->setTopRightTile(tile(image, QRect(right - r, 0, padRight + r, padTop + r)));
    m_shadow->setRightTile(tile(image, QRect(right, midY, padRight, 1)));
    m_shadow->setBottomRightTile(tile(image, QRect(right - r, bottom - r, padRight + r, padBottom + r)));
    m_shadow->setBottomTile(tile(image, QRect(midX, bottom, 1, padBottom)));
    m_shadow->setBottomLeftTile(tile(image, QRect(0, bottom - r, padLeft + r, padBottom + r)));
    m_shadow->setLeftTile(tile(image, QRect(0, midY, padLeft, 1)));
    m_shadow->setPadding(QMargins(padLeft, padTop, padRight, padBottom));
    m_shadow->setWindow(m_window);
    m_shadow->create();
}

void WindowFrame::applyBlur(bool full)
{
    const bool on = m_blurOn && m_blurAvailable;
    const QSize size = m_window->size();
    const int radius = full ? 0 : m_radius;
    if (on && size == m_blurSize && radius == m_blurRadius) {
        return;
    }
    KWindowEffects::enableBlurBehind(m_window, on, on ? roundedRegion(size, radius) : QRegion());
    m_blurSize = on ? size : QSize();
    m_blurRadius = radius;
}
