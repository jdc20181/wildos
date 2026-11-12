#include "windowframe.h"

#include <KWindowEffects>
#include <KWindowShadow>
#include <QEvent>
#include <QImage>
#include <QTimer>
#include <cmath>

namespace
{
// shadow-window from the design tokens: 0 30px 80px rgba(10,12,16,0.45). A CSS
// blur of 80px is a Gaussian with a sigma of 40, which has faded to nothing
// two sigmas out.
constexpr int shadowOffset = 30;
constexpr double shadowSigma = 40.0;
constexpr int shadowReach = 80;
constexpr double shadowAlpha = 0.45;
const QColor shadowColour(10, 12, 16);

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

KWindowShadowTile::Ptr tile(const QImage &image, const QRect &area)
{
    auto t = KWindowShadowTile::Ptr::create();
    t->setImage(image.copy(area));
    t->create();
    return t;
}
}

QRegion WindowFrame::roundedRegion(const QSize &size, int radius, bool roundBottom)
{
    if (radius <= 0) {
        return QRegion(QRect(QPoint(0, 0), size));
    }
    QRegion region(QRect(0, radius, size.width(), size.height() - (roundBottom ? 2 : 1) * radius));
    for (int row = 0; row < radius; ++row) {
        const double dy = radius - row - 0.5;
        const int inset = int(std::ceil(radius - std::sqrt(std::max(0.0, double(radius * radius) - dy * dy))));
        region += QRect(inset, row, size.width() - 2 * inset, 1);
        if (roundBottom) {
            region += QRect(inset, size.height() - 1 - row, size.width() - 2 * inset, 1);
        } else {
            region += QRect(0, size.height() - 1 - row, size.width(), 1);
        }
    }
    return region;
}

WindowFrame::WindowFrame(QWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    m_blurAvailable = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
    m_window->installEventFilter(this);
    connect(m_window, &QWindow::visibleChanged, this, &WindowFrame::schedule);
    connect(m_window, &QWindow::widthChanged, this, &WindowFrame::schedule);
    connect(m_window, &QWindow::heightChanged, this, &WindowFrame::schedule);
    schedule();
}

WindowFrame::~WindowFrame() = default;

bool WindowFrame::eventFilter(QObject *watched, QEvent *event)
{
    // Shadow and blur belong to the surface; a recreated surface needs
    // them again.
    if (watched == m_window && event->type() == QEvent::PlatformSurface) {
        if (m_shadow) {
            m_shadow->destroy();
        }
        m_blurApplied = false;
        schedule();
    }
    return QObject::eventFilter(watched, event);
}

void WindowFrame::setRadius(int radius)
{
    if (m_radius != radius) {
        m_radius = radius;
        if (m_shadow) {
            m_shadow->destroy();
        }
        schedule();
    }
}

void WindowFrame::setShadow(bool on)
{
    if (m_shadowOn != on) {
        m_shadowOn = on;
        schedule();
    }
}

void WindowFrame::setBlurRegion(const QRegion &region)
{
    if (region != m_blurRegion) {
        m_blurRegion = region;
        schedule();
    }
}

bool WindowFrame::recheck()
{
    const bool available = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
    if (available != m_blurAvailable) {
        m_blurAvailable = available;
        m_blurApplied = false;
        schedule();
        return true;
    }
    return false;
}

// Resizing sends a burst of size changes; one update per pass of the event
// loop is plenty for the compositor.
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
    if (!m_window || !m_window->isVisible() || !m_window->handle()) {
        return;
    }
    applyShadow();
    const QRegion blur = m_blurAvailable ? m_blurRegion : QRegion();
    if (!m_blurApplied || blur != m_appliedBlur) {
        KWindowEffects::enableBlurBehind(m_window, !blur.isEmpty(), blur);
        m_appliedBlur = blur;
        m_blurApplied = true;
    }
}

void WindowFrame::applyShadow()
{
    if (!m_shadowOn) {
        if (m_shadow) {
            m_shadow->destroy();
            delete m_shadow;
            m_shadow = nullptr;
        }
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
            const double a = shadowAlpha * fy * coverage(cx, windowRect.left(), windowRect.right());
            const int alpha = std::clamp(int(std::lround(a * 255.0)), 0, 255);
            line[x] = qPremultiply(qRgba(shadowColour.red(), shadowColour.green(), shadowColour.blue(), alpha));
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
