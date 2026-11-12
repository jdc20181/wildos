#include "thumbnails.h"

QMutex Thumbnails::s_mutex;
QHash<QString, QImage> Thumbnails::s_images;

void Thumbnails::insert(const QString &key, const QImage &image)
{
    QMutexLocker lock(&s_mutex);
    s_images.insert(key, image);
}

QImage Thumbnails::find(const QString &key)
{
    QMutexLocker lock(&s_mutex);
    return s_images.value(key);
}

void Thumbnails::drop(const QString &archiveKey)
{
    QMutexLocker lock(&s_mutex);
    const QString prefix = archiveKey + QLatin1Char('/');
    for (auto it = s_images.begin(); it != s_images.end();) {
        if (it.key().startsWith(prefix)) {
            it = s_images.erase(it);
        } else {
            ++it;
        }
    }
}

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image = Thumbnails::find(id);
    if (!image.isNull() && requestedSize.isValid() && (image.width() > requestedSize.width() * 1.5 || image.height() > requestedSize.height() * 1.5)) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}
