#pragma once

#include <QQuickImageProvider>
#include <QThreadPool>

// image://page/<generation>/<page>/<pixel width>/<revision>
//
// Pages are drawn by Poppler on a thread of their own so scrolling never
// waits for them. The revision is only there to make a new URL when a page
// changes (a note added, a field filled in); QML's image cache keeps every
// page already drawn at that size until then.
class PageProvider : public QQuickAsyncImageProvider
{
public:
    PageProvider();
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QThreadPool m_pool;
};
