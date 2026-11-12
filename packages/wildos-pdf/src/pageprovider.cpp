#include "pageprovider.h"

#include "document.h"

#include <QRunnable>

#include <atomic>

namespace
{
class PageResponse : public QQuickImageResponse, public QRunnable
{
public:
    PageResponse(int generation, int page, int width)
        : m_generation(generation)
        , m_page(page)
        , m_width(width)
    {
        // QML deletes the response once it has finished, not the pool.
        setAutoDelete(false);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void run() override
    {
        // A page scrolled past before its turn came is never drawn.
        if (!m_cancelled.load()) {
            m_image = PdfDocument::instance()->render(m_generation, m_page, m_width);
        }
        Q_EMIT finished();
    }

    void cancel() override
    {
        m_cancelled.store(true);
    }

private:
    int m_generation;
    int m_page;
    int m_width;
    QImage m_image;
    std::atomic_bool m_cancelled{false};
};
}

PageProvider::PageProvider()
{
    // Poppler draws one page at a time anyway (PdfDocument's lock); two
    // threads keep a thumbnail from waiting behind a large page for long.
    m_pool.setMaxThreadCount(2);
}

QQuickImageResponse *PageProvider::requestImageResponse(const QString &id, const QSize &)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    auto *response = new PageResponse(parts.value(0).toInt(), parts.value(1).toInt(), parts.value(2).toInt());
    m_pool.start(response);
    return response;
}
