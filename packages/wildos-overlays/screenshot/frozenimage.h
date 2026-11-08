#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// The screens as they were when Win+Shift+S was pressed, shown under the
// selection so what you select is exactly what you get, even if a video
// keeps playing behind it. image://frozen/<screen name>/<serial>.
class FrozenImages : public QQuickImageProvider
{
public:
    FrozenImages();

    void set(const QHash<QString, QImage> &images);
    QImage image(const QString &screen) const;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    mutable QMutex m_lock;
    QHash<QString, QImage> m_images;
};
