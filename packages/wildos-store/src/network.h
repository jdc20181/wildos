#pragma once

#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>

// The network access QML's Image uses for remote icons and screenshots.
// Every fetch is logged with its host and time, so slow pages can be put
// down to Flathub's CDN or Debian's screenshot server.
class StoreNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    explicit StoreNetworkAccessManager(QObject *parent = nullptr);

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override;
};

class StoreNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
};
