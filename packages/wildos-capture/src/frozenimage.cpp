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
    return m_kept.contains(screen) ? m_kept.value(screen) : m_images.value(screen);
}

QString FrozenImages::keep(const QImage &capture)
{
    QMutexLocker locker(&m_lock);
    const QString key = QStringLiteral("capture-%1").arg(++m_next);
    // The editor works in the picture's own pixels, as it does for a file.
    QImage plain = capture;
    plain.setDevicePixelRatio(1);
    m_kept.insert(key, plain);
    return key;
}

void FrozenImages::release(const QString &key)
{
    QMutexLocker locker(&m_lock);
    m_kept.remove(key);
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
