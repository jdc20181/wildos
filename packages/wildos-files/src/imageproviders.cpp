#include "imageproviders.h"

#include <KFileItem>
#include <KIO/PreviewJob>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPointer>
#include <QSvgRenderer>
#include <QUrl>

namespace
{
QString smallIconPath(const QString &name)
{
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(name);
    if (it != cache.constEnd()) {
        return it.value();
    }
    QString found;
    const QStringList dirs = QIcon::themeSearchPaths();
    for (const QString &dir : dirs) {
        for (const char *context : {"mimetypes", "places", "apps"}) {
            const QString path = QStringLiteral("%1/%2/16x16/%3/%4.svg").arg(dir, QIcon::themeName(), QLatin1String(context), name);
            if (QFile::exists(path)) {
                found = path;
                break;
            }
        }
        if (!found.isEmpty()) {
            break;
        }
    }
    cache.insert(name, found);
    return found;
}

QStringList candidates(const QString &id)
{
    QStringList names;
    const QStringList parts = id.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        // The icon naming spec's fallbacks: x-office-spreadsheet, then
        // x-office, then x.
        QString name = part;
        while (!name.isEmpty()) {
            names.append(name);
            const auto dash = name.lastIndexOf(QLatin1Char('-'));
            if (dash <= 0) {
                break;
            }
            name.truncate(dash);
        }
    }
    names.append(QStringLiteral("text-x-generic"));
    return names;
}
}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int side = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 48;
    if (size) {
        *size = QSize(side, side);
    }
    const QStringList names = candidates(QUrl::fromPercentEncoding(id.toUtf8()));
    if (side <= 24) {
        for (const QString &name : names) {
            const QString path = smallIconPath(name);
            if (path.isEmpty()) {
                if (QIcon::hasThemeIcon(name)) {
                    break;
                }
                continue;
            }
            QSvgRenderer svg(path);
            QPixmap pixmap(side, side);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            svg.render(&painter, QRectF(0, 0, side, side));
            return pixmap;
        }
    }
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name).pixmap(side, side);
        }
    }
    return QIcon::fromTheme(QStringLiteral("unknown")).pixmap(side, side);
}

namespace
{
class ThumbnailResponse : public QQuickImageResponse
{
public:
    ThumbnailResponse(const QUrl &url, const QSize &size)
    {
        // KIO jobs belong to the main thread.
        QMetaObject::invokeMethod(QCoreApplication::instance(), [this, url, size, guard = QPointer<ThumbnailResponse>(this)] {
            if (!guard) {
                return;
            }
            const KFileItem item(url);
            auto job = KIO::filePreview(KFileItemList{item}, size.isValid() ? size : QSize(256, 256));
            job->setScaleType(KIO::PreviewJob::ScaledAndCached);
            job->setIgnoreMaximumSize(false);
            m_job = job;
            QObject::connect(job, &KIO::PreviewJob::gotPreview, job, [guard](const KFileItem &, const QPixmap &pixmap) {
                if (guard) {
                    guard->m_image = pixmap.toImage();
                }
            });
            QObject::connect(job, &KJob::result, job, [guard] {
                if (guard) {
                    Q_EMIT guard->finished();
                }
            });
        }, Qt::QueuedConnection);
    }

    ~ThumbnailResponse() override
    {
        stop();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_image.isNull() ? QStringLiteral("No preview") : QString();
    }

    void cancel() override
    {
        stop();
    }

private:
    void stop()
    {
        // Responses can die on the image loader thread; the job lives on
        // the main one.
        if (KIO::PreviewJob *job = m_job.data()) {
            QMetaObject::invokeMethod(job, [job = QPointer<KIO::PreviewJob>(job)] {
                if (job) {
                    job->kill();
                }
            }, Qt::QueuedConnection);
        }
    }

    QImage m_image;
    QPointer<KIO::PreviewJob> m_job;
};
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // The id is "<stamp>/<url>": the stamp changes with the file, so the
    // image cache never serves a stale picture.
    const QString encoded = id.section(QLatin1Char('/'), 1);
    const QUrl url(QUrl::fromPercentEncoding(encoded.toUtf8()));
    return new ThumbnailResponse(url, requestedSize);
}
