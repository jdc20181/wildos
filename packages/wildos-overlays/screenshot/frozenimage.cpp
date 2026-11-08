#include "frozenimage.h"

FrozenImages::FrozenImages()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

void FrozenImages::set(const QHash<QString, QImage> &images)
{
    QMutexLocker locker(&m_lock);
    m_images = images;
}

QImage FrozenImages::image(const QString &screen) const
{
    QMutexLocker locker(&m_lock);
    return m_images.value(screen);
}

QImage FrozenImages::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString screen = id.section(QLatin1Char('/'), 0, 0);
    QImage result = image(screen);
    if (result.isNull()) {
        result = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
    }
    if (requestedSize.isValid() && requestedSize != result.size()) {
        result = result.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = result.size();
    }
    return result;
}
