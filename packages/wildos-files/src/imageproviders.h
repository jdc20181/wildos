#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>

// image://icon/<name>[,<fallback>]: a Wild OS theme icon. At list sizes
// it uses the theme's simplified 16 px drawings, scaled crisply.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

// image://thumb/<url>: a KIO preview (photos, videos, PDFs...) made in
// the background; the icon stays until it arrives.
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
