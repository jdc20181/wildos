#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// Small pictures of the images inside open archives, for the rows and the
// extract pane: image://archivethumb/<archive key>/<order>. They are made
// in memory from the archive and never written to disk.
class Thumbnails
{
public:
    static void insert(const QString &key, const QImage &image);
    static QImage find(const QString &key);
    // Forgets every picture of one archive.
    static void drop(const QString &archiveKey);

private:
    static QMutex s_mutex;
    static QHash<QString, QImage> s_images;
};

class ThumbnailProvider : public QQuickImageProvider
{
public:
    ThumbnailProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
