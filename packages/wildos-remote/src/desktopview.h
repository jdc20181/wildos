#pragma once

#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QRegion>

#include "remotedesktop.h"

// The remote desktop's picture, and the keyboard and pointer going to it
// (RemoteSession.dc, RemoteWindowed.dc). It shows `sourceRect` of the
// desktop (all of it unless set: one monitor's part with all monitors),
// filling the item at the same aspect, and maps input back to the desktop's
// pixels.
//
// Ctrl+Alt+Home never goes to the remote PC: it brings the toolbar, and
// gives the desktop's own shortcuts back while "Send all shortcuts there"
// is on.
class DesktopView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(RemoteDesktop *desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QRect sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(bool blurred READ blurred WRITE setBlurred NOTIFY blurredChanged)
    Q_PROPERTY(bool hasPicture READ hasPicture NOTIFY pictureChanged)

public:
    explicit DesktopView(QQuickItem *parent = nullptr);

    RemoteDesktop *desktop() const { return m_desktop; }
    void setDesktop(RemoteDesktop *desktop);
    QRect sourceRect() const { return m_sourceRect; }
    void setSourceRect(const QRect &rect);
    bool blurred() const { return m_blurred; }
    void setBlurred(bool blurred);
    bool hasPicture() const { return !m_image.isNull(); }

    // The part of the item the picture covers.
    Q_INVOKABLE QRectF pictureRect() const;

Q_SIGNALS:
    void desktopChanged();
    void sourceRectChanged();
    void blurredChanged();
    void pictureChanged();
    // Ctrl+Alt+Home.
    void escapeRequested();
    void pointerMoved(qreal x, qreal y);

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void damaged(const QRegion &region);
    QRect source() const;
    QPoint toRemote(const QPointF &local) const;

    QPointer<RemoteDesktop> m_desktop;
    QRect m_sourceRect;
    QImage m_image;
    bool m_dirty = false;
    bool m_blurred = false;
    QList<quint32> m_keysDown;
    int m_wheelX = 0;
    int m_wheelY = 0;
};
