#include "modeldownload.h"

#include "paths.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStorageInfo>

namespace SpeechModel
{
QString path()
{
    return QDir(VoicePaths::modelFolder()).filePath(QString::fromLatin1(name));
}

bool verify(const QString &file)
{
    QFile model(file);
    if (model.size() != size || !model.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&model)) {
        return false;
    }
    return hash.result().toHex() == QByteArray(sha256);
}
}

ModelDownload::ModelDownload(QObject *parent)
    : QObject(parent)
{
}

ModelDownload::~ModelDownload()
{
    abandon();
}

void ModelDownload::start()
{
    if (running()) {
        return;
    }
    const QString folder = VoicePaths::modelFolder();
    if (!QDir().mkpath(folder)) {
        Q_EMIT finished(false, tr("Voice couldn't make a folder for the speech model."));
        return;
    }
    if (QStorageInfo(folder).bytesAvailable() < SpeechModel::size + 64 * 1024 * 1024) {
        Q_EMIT finished(false, tr("There isn't enough free space for the speech model, which needs 142 MB."));
        return;
    }
    m_file.setFileName(SpeechModel::path() + QStringLiteral(".part"));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT finished(false, tr("Voice couldn't save the speech model."));
        return;
    }
    m_hash.reset();
    m_received = 0;
    // Made only when the model is fetched: nothing else in Voice has a way
    // out to the network.
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    QNetworkRequest request{QUrl(QString::fromLatin1(SpeechModel::url))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Wild OS Voice"));
    request.setTransferTimeout(60000);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &ModelDownload::take);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
        Q_EMIT progress(received, SpeechModel::size);
    });
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (!m_reply) {
            return;
        }
        take();
        if (!m_reply) {
            return;
        }
        const QNetworkReply::NetworkError error = m_reply->error();
        m_reply->deleteLater();
        m_reply = nullptr;
        if (error == QNetworkReply::OperationCanceledError) {
            return;
        }
        m_file.close();
        if (error != QNetworkReply::NoError) {
            m_file.remove();
            Q_EMIT finished(false, tr("The speech model couldn't be downloaded. Check your internet connection and try again."));
            return;
        }
        if (m_file.size() != SpeechModel::size || m_hash.result().toHex() != QByteArray(SpeechModel::sha256)) {
            m_file.remove();
            Q_EMIT finished(false, tr("The download wasn't the speech model Voice expected, so it was deleted. Try again later."));
            return;
        }
        QFile::remove(SpeechModel::path());
        if (!m_file.rename(SpeechModel::path())) {
            m_file.remove();
            Q_EMIT finished(false, tr("Voice couldn't save the speech model."));
            return;
        }
        Q_EMIT finished(true, QString());
    });
}

void ModelDownload::take()
{
    const QByteArray data = m_reply->readAll();
    m_received += data.size();
    if (m_received > SpeechModel::size || m_file.write(data) != data.size()) {
        const bool tooBig = m_received > SpeechModel::size;
        abandon();
        Q_EMIT finished(false, tooBig ? tr("The download wasn't the speech model Voice expected, so it was deleted.")
                                      : tr("Voice couldn't save the speech model. The disk may be full."));
        return;
    }
    m_hash.addData(data);
}

void ModelDownload::cancel()
{
    if (!running()) {
        return;
    }
    abandon();
    Q_EMIT finished(false, QString());
}

void ModelDownload::abandon()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    // Only ever the .part file: the finished model is never touched here.
    if (m_file.isOpen()) {
        m_file.close();
        m_file.remove();
    }
}
