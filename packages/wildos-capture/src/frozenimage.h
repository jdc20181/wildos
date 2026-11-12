#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// The screens as they were when Print Screen was pressed, shown under the
// selection so what you select is exactly what you get, even if a video
// keeps playing behind it: image://frozen/<screen name>/<serial>. Also each
// capture an editor window has open, image://frozen/<key>, until it closes.
class FrozenImages : public QQuickImageProvider
{
public:
    FrozenImages();

    void set(const QHash<QString, QImage> &images);
    QImage image(const QString &screen) const;
    QString keep(const QImage &capture);
    void release(const QString &key);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    mutable QMutex m_lock;
    QHash<QString, QImage> m_images;
    QHash<QString, QImage> m_kept;
    int m_next = 0;
};
