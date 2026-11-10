#include "network.h"
#include "timing.h"

#include <QMutex>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

namespace
{
// Addresses that answered 404 this session. Debian's screenshot links often
// point at pictures that have been pruned; asking again only spins.
QMutex missingMutex;
QSet<QUrl> missing;

// Answers at once, as the server did the first time.
class MissingReply : public QNetworkReply
{
public:
    MissingReply(const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
        setError(ContentNotFoundError, QStringLiteral("Not found"));
        open(ReadOnly);
        setFinished(true);
        QTimer::singleShot(0, this, [this] {
            Q_EMIT errorOccurred(ContentNotFoundError);
            Q_EMIT finished();
        });
    }

    void abort() override
    {
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }
};
}

StoreNetworkAccessManager::StoreNetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
    // Icons and screenshots on Flathub and Debian live at addresses that
    // change whenever the picture does, so a copy on disk never goes stale.
    auto cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/wildos-store/images"));
    cache->setMaximumCacheSize(qint64(400) * 1024 * 1024);
    setCache(cache);
}

QNetworkReply *StoreNetworkAccessManager::createRequest(Operation op, const QNetworkRequest &original, QIODevice *outgoingData)
{
    QNetworkRequest request(original);
    if (op == GetOperation) {
        {
            QMutexLocker lock(&missingMutex);
            if (missing.contains(request.url())) {
                return new MissingReply(request, this);
            }
        }
        // Use the copy on disk whatever the server's caching headers say;
        // see the constructor.
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    }
    QNetworkReply *reply = QNetworkAccessManager::createRequest(op, request, outgoingData);
    const qint64 began = storeClock();
    connect(reply, &QNetworkReply::finished, reply, [reply, began] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404 || status == 410) {
            QMutexLocker lock(&missingMutex);
            missing.insert(reply->request().url());
        }
        if (!lcStore().isInfoEnabled()) {
            return;
        }
        const bool cached = reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool();
        qCInfo(lcStore).noquote() << "image:" << reply->request().url().host() << storeClock() - began << "ms"
                                  << reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() << "bytes"
                                  << (cached ? "from disk cache" : "from network") << status << reply->url().toString();
    });
    return reply;
}

QNetworkAccessManager *StoreNetworkFactory::create(QObject *parent)
{
    return new StoreNetworkAccessManager(parent);
}
