#pragma once

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QObject>
#include <QString>

#include <atomic>
#include <vector>

struct whisper_context;

// Lives on the transcript thread and runs whisper.cpp there, so the window
// never waits on it. Sound comes in at 16 kHz and is written down in
// pieces of 8 to 15 seconds while recording (28 when transcribing a file),
// cut where it is quietest so words aren't split. Each line goes to the
// recording's .txt as it is made, so a crash keeps what was done.
class TranscriberWorker : public QObject
{
    Q_OBJECT

public:
    explicit TranscriberWorker(QObject *parent = nullptr);
    ~TranscriberWorker() override;

    // Starts writing `path`'s transcript, its first line `offsetMs` into
    // the recording.
    void begin(const QString &path, qint64 offsetMs, bool live);
    void feed(const QList<float> &samples);
    // Writes down what is left and closes the transcript.
    void end();
    // Transcribes a finished recording from start to end.
    void transcribeFile(const QString &path);
    void unloadModel();

    // Set from any thread: stops the current piece as soon as Whisper
    // allows. `quitting` stops everything for good.
    std::atomic<bool> abortFile{false};
    std::atomic<bool> quitting{false};

Q_SIGNALS:
    void line(const QString &path, qint64 ms, const QString &text);
    void progress(const QString &path, double fraction);
    void done(const QString &path, bool complete);
    void failed(const QString &message);
    void modelDamaged();

private:
    bool loadModel();
    void process(size_t count);
    size_t quietestCut(size_t from, size_t to) const;
    void finishSession(bool complete);

    whisper_context *m_context = nullptr;
    bool m_verified = false;
    int m_threads = 2;

    QString m_path;
    QFile m_out;
    bool m_live = false;
    bool m_open = false;
    std::vector<float> m_buffer;
    // Samples already written down, from the start of the transcript.
    qint64 m_done = 0;
    qint64 m_offsetMs = 0;
    QByteArray m_language;
};
