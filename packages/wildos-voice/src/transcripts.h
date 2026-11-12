#pragma once

#include "modeldownload.h"

#include <QObject>
#include <QQmlEngine>
#include <QThread>
#include <QVariantList>

class TranscriberWorker;

// Transcripts, made on this PC with whisper.cpp. Off until turned on;
// turning them on fetches the speech model once (see SpeechModel). While
// recording, lines appear as each piece is written down; a recording made
// with transcripts off can be transcribed afterwards.
class Transcripts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // False on a processor without AVX2, FMA and F16C, which the speech
    // model needs.
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool modelReady READ modelReady NOTIFY modelChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadChanged)
    // Something to tell the person, such as why the download failed.
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    // The recording being written down now, live or afterwards.
    Q_PROPERTY(QString workingPath READ workingPath NOTIFY workingChanged)
    // How far a transcript of a finished recording has got, 0 to 1.
    Q_PROPERTY(double fileProgress READ fileProgress NOTIFY workingChanged)
    // The recording whose transcript `lines` holds: [{ time (ms), text }].
    Q_PROPERTY(QString shownPath READ shownPath WRITE setShownPath NOTIFY linesChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)

public:
    explicit Transcripts(QObject *parent);
    ~Transcripts() override;
    static Transcripts *instance();
    static Transcripts *create(QQmlEngine *, QJSEngine *);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool supported() const { return m_supported; }
    bool modelReady() const { return m_modelReady; }
    bool downloading() const { return m_download.running(); }
    double downloadProgress() const { return m_downloadProgress; }
    QString message() const { return m_message; }
    QString workingPath() const { return m_workingPath; }
    double fileProgress() const { return m_fileProgress; }
    QString shownPath() const { return m_shownPath; }
    void setShownPath(const QString &path);
    QVariantList lines() const { return m_lines; }

    Q_INVOKABLE void transcribe(const QString &path);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void clearMessage();
    // Stops at once, for quitting: what is written so far stays.
    void shutDown();

    void recordingStarted(const QString &path);
    void recordingFinished(const QString &path);
    void speech(const QList<float> &samples);

Q_SIGNALS:
    void enabledChanged();
    void modelChanged();
    void downloadChanged();
    void messageChanged();
    void workingChanged();
    void linesChanged();

private:
    void setMessage(const QString &message);
    void beginLive(const QString &path, qint64 offsetMs);
    void endLive();
    void reloadLines();
    void setWorking(const QString &path, double progress);

    QThread m_thread;
    TranscriberWorker *m_worker = nullptr;
    ModelDownload m_download;
    bool m_enabled = false;
    bool m_supported = false;
    bool m_modelReady = false;
    double m_downloadProgress = 0;
    QString m_message;
    QString m_livePath;
    QString m_workingPath;
    double m_fileProgress = -1;
    QString m_shownPath;
    QVariantList m_lines;
};
