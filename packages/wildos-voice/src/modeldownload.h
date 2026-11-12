#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>

class QNetworkAccessManager;

// The speech model Whisper needs: OpenAI's Whisper base model, multilingual,
// in whisper.cpp's ggml format. It is the only thing Voice ever fetches
// from the internet, from one fixed address (a fixed revision of the
// whisper.cpp model repository on Hugging Face), and only when transcripts
// are turned on. It is kept only if its SHA-256 matches the one here.
namespace SpeechModel
{
constexpr const char *name = "ggml-base.bin";
constexpr const char *url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/5359861c739e955e79d9a303bcbc70fb988958b1/ggml-base.bin";
constexpr const char *sha256 = "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe";
constexpr qint64 size = 147951465;

QString path();
// Hashes the whole file: about a third of a second, so off the window's
// thread.
bool verify(const QString &file);
}

// Fetches the model into a .part file beside where it goes, checking it as
// it comes, and moves it into place only when the checksum matches.
class ModelDownload : public QObject
{
    Q_OBJECT

public:
    explicit ModelDownload(QObject *parent = nullptr);
    ~ModelDownload() override;

    bool running() const { return m_reply != nullptr; }
    void start();
    void cancel();

Q_SIGNALS:
    void progress(qint64 received, qint64 total);
    // ok is false with a message to show, or with none when cancelled.
    void finished(bool ok, const QString &message);

private:
    void take();
    void abandon();

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_received = 0;
};
