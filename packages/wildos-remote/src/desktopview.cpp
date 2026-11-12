#include "desktopview.h"
#include "remotedesktop.h"

#include "client.h"
#include "desktop.h"
#include "input.h"

#include <QCursor>
#include <QPainter>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTexture>

#include <cstring>

DesktopView::DesktopView(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setActiveFocusOnTab(true);
}

void DesktopView::setDesktop(RemoteDesktop *desktop)
{
    if (desktop == m_desktop) {
        return;
    }
    if (m_desktop && m_desktop->session()) {
        m_desktop->session()->disconnect(this);
    }
    m_desktop = desktop;
    if (desktop && desktop->session()) {
        DesktopSession *s = desktop->session();
        connect(s, &DesktopSession::frameChanged, this, &DesktopView::damaged);
        connect(s, &DesktopSession::pointerImage, this, [this](const QImage &image, const QPoint &hotspot) {
            // Windows sends the pointer at its own pixels; the screen's
            // scale makes it the right size here.
            QPixmap pixmap = QPixmap::fromImage(image);
            const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
            pixmap.setDevicePixelRatio(dpr);
            setCursor(QCursor(pixmap, int(hotspot.x() / dpr), int(hotspot.y() / dpr)));
        });
        connect(s, &DesktopSession::pointerHidden, this, [this] { setCursor(Qt::BlankCursor); });
        connect(s, &DesktopSession::pointerDefault, this, [this] { setCursor(Qt::ArrowCursor); });
        // A picture already drawn (a second monitor's window opening later).
        const QImage frame = s->frame();
        if (!frame.isNull()) {
            m_image = frame;
            m_dirty = true;
            update();
            Q_EMIT pictureChanged();
        }
    }
    Q_EMIT desktopChanged();
}

void DesktopView::setSourceRect(const QRect &rect)
{
    if (rect != m_sourceRect) {
        m_sourceRect = rect;
        update();
        Q_EMIT sourceRectChanged();
    }
}

void DesktopView::setBlurred(bool blurred)
{
    if (blurred != m_blurred) {
        m_blurred = blurred;
        m_dirty = true;
        update();
        Q_EMIT blurredChanged();
    }
}

void DesktopView::damaged(const QRegion &region)
{
    if (!m_desktop || !m_desktop->session()) {
        return;
    }
    const bool had = !m_image.isNull();
    m_desktop->session()->client()->withPrimary([this, &region](const uchar *data, int stride, QSize size) {
        if (m_image.size() != size) {
            // A new desktop size: the whole picture again.
            m_image = QImage(data, size.width(), size.height(), stride, QImage::Format_RGB32).copy();
            return;
        }
        for (const QRect &r : region) {
            const QRect c = r.intersected(QRect(QPoint(0, 0), size));
            for (int y = c.top(); y <= c.bottom(); ++y) {
                std::memcpy(m_image.scanLine(y) + qsizetype(c.left()) * 4, data + qsizetype(y) * stride + qsizetype(c.left()) * 4,
                            size_t(c.width()) * 4);
            }
        }
    });
    m_dirty = true;
    update();
    if (!had) {
        Q_EMIT pictureChanged();
    }
}

QRect DesktopView::source() const
{
    const QRect all(QPoint(0, 0), m_image.size());
    return m_sourceRect.isValid() ? m_sourceRect.intersected(all) : all;
}

QRectF DesktopView::pictureRect() const
{
    const QRect src = source();
    if (src.isEmpty() || width() <= 0 || height() <= 0) {
        return {};
    }
    const double s = std::min(width() / src.width(), height() / src.height());
    const QSizeF shown(src.width() * s, src.height() * s);
    return QRectF(QPointF((width() - shown.width()) / 2, (height() - shown.height()) / 2), shown);
}

QPoint DesktopView::toRemote(const QPointF &local) const
{
    const QRectF pic = pictureRect();
    const QRect src = source();
    if (pic.isEmpty()) {
        return {};
    }
    const double x = (local.x() - pic.x()) / pic.width() * src.width() + src.x();
    const double y = (local.y() - pic.y()) / pic.height() * src.height() + src.y();
    return QPoint(std::clamp(int(x), 0, std::max(0, m_image.width() - 1)), std::clamp(int(y), 0, std::max(0, m_image.height() - 1)));
}

QSGNode *DesktopView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGImageNode *>(old);
    if (m_image.isNull() || !window()) {
        delete node;
        return nullptr;
    }
    if (!node) {
        node = window()->createImageNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
    }
    if (m_dirty || !node->texture()) {
        QImage shown = m_image;
        if (m_blurred) {
            // Behind "Connection lost": soft, so the question reads.
            const QSize small = m_image.size() / 16;
            shown = m_image.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .scaled(m_image.size() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QPainter p(&shown);
            p.fillRect(shown.rect(), QColor(0, 0, 0, 110));
        }
        node->setTexture(window()->createTextureFromImage(shown));
        m_dirty = false;
    }
    const QRect src = source();
    const double ratio = m_blurred ? 0.5 : 1.0;
    node->setSourceRect(QRectF(src.x() * ratio, src.y() * ratio, src.width() * ratio, src.height() * ratio));
    node->setRect(pictureRect());
    return node;
}

void DesktopView::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::button(m_desktop->session()->client(), event->button(), true, toRemote(event->position()));
    }
    event->accept();
}

void DesktopView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::button(m_desktop->session()->client(), event->button(), false, toRemote(event->position()));
    }
}

void DesktopView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::move(m_desktop->session()->client(), toRemote(event->position()));
    }
    Q_EMIT pointerMoved(event->position().x(), event->position().y());
}

void DesktopView::hoverMoveEvent(QHoverEvent *event)
{
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::move(m_desktop->session()->client(), toRemote(event->position()));
    }
    Q_EMIT pointerMoved(event->position().x(), event->position().y());
}

void DesktopView::wheelEvent(QWheelEvent *event)
{
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::wheel(m_desktop->session()->client(), event->angleDelta(), toRemote(event->position()), m_wheelX, m_wheelY);
    }
}

void DesktopView::keyPressEvent(QKeyEvent *event)
{
    const auto mods = event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (event->key() == Qt::Key_Home && mods == (Qt::ControlModifier | Qt::AltModifier)) {
        // Ctrl and Alt are already down over there: they come up first.
        if (m_desktop && m_desktop->session()) {
            rdpinput::releaseAll(m_desktop->session()->client(), m_keysDown);
        }
        Q_EMIT escapeRequested();
        return;
    }
    if (m_desktop && m_desktop->session() && !m_blurred) {
        rdpinput::key(m_desktop->session()->client(), event, true, m_keysDown);
    }
}

void DesktopView::keyReleaseEvent(QKeyEvent *event)
{
    if (m_desktop && m_desktop->session()) {
        rdpinput::key(m_desktop->session()->client(), event, false, m_keysDown);
    }
}

void DesktopView::focusOutEvent(QFocusEvent *event)
{
    if (m_desktop && m_desktop->session()) {
        rdpinput::releaseAll(m_desktop->session()->client(), m_keysDown);
    }
    QQuickItem::focusOutEvent(event);
}

void DesktopView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}
