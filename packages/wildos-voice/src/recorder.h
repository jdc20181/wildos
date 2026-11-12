#pragma once

#include "audio.h"
#include "opuswriter.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QMediaDevices>
#include <QObject>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include <memory>
#include <vector>

class QAudioSource;
class QIODevice;

// Lives on the recording thread: reads the microphone, writes the Opus
// file, measures the waveform bars and passes 16 kHz sound on for the
// transcript. Nothing here waits on the window.
class RecorderWorker : public QObject
{
    Q_OBJECT

public:
    explicit RecorderWorker(QObject *parent = nullptr);
    ~RecorderWorker() override;

    void start(const QAudioDevice &device, const QString &path, const QString &title);
    void setPaused(bool paused);
    void stop();

Q_SIGNALS:
    void started();
    // New bars since the last signal, and the length recorded so far.
    void bars(const QList<float> &levels, qint64 elapsedMs);
    // About a second of 16 kHz mono sound at a time.
    void speech(const QList<float> &samples);
    // True after a few seconds of nothing at all from the microphone, as
    // when it is muted.
    void silentChanged(bool silent);
    void stopped(qint64 elapsedMs);
    void failed(const QString &message);

private:
    void readAvailable();
    void consume(const float *mono, int frames);
    void finish(const QString &failure);

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    // The sound system doesn't always report a microphone that fails to
    // open or goes away; sound that stops coming is how it shows.
    QTimer m_watchdog;
    QElapsedTimer m_sinceData;
    QAudioFormat m_format;
    OpusWriter m_writer;
    std::unique_ptr<Audio::Downsampler> m_down;
    std::vector<float> m_mono;
    std::vector<float> m_speech;
    QList<float> m_pendingBars;
    qint64 m_frames = 0;
    int m_perBar = 4800;
    int m_inBar = 0;
    double m_energy = 0;
    qint64 m_quietFrames = 0;
    bool m_silent = false;
    bool m_paused = false;
};

// The recording in progress, for the window: start, pause, mark and stop,
// the microphone to use, and the waveform so far.
class Recorder : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "idle", "recording" or "paused".
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString path READ path NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY elapsedChanged)
    // The latest bars, oldest first, enough to fill the widest waveform.
    Q_PROPERTY(QVariantList recentBars READ recentBars NOTIFY elapsedChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    // [{ id, name }]; an empty id is the one chosen in Settings > Sound.
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY devicesChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY devicesChanged)
    Q_PROPERTY(bool hasMicrophone READ hasMicrophone NOTIFY devicesChanged)
    Q_PROPERTY(bool silent READ silent NOTIFY silentChanged)

public:
    explicit Recorder(QObject *parent);
    ~Recorder() override;
    static Recorder *instance();
    static Recorder *create(QQmlEngine *, QJSEngine *);

    QString state() const { return m_state; }
    bool active() const { return m_state != QLatin1String("idle"); }
    QString path() const { return m_path; }
    QString title() const { return m_title; }
    double elapsed() const { return double(m_elapsed); }
    QVariantList recentBars() const;
    QVariantList markers() const;
    QVariantList devices() const { return m_devices; }
    QString deviceId() const { return m_deviceId; }
    void setDeviceId(const QString &id);
    QString deviceName() const;
    bool hasMicrophone() const { return !m_devices.isEmpty(); }
    bool silent() const { return m_silent; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void mark();
    // Stops and waits for the file to be finished, for quitting.
    void stopNow();

Q_SIGNALS:
    void stateChanged();
    void elapsedChanged();
    void markersChanged();
    void devicesChanged();
    void silentChanged();
    void started(const QString &path);
    void finished(const QString &path);
    void failed(const QString &message);
    void speech(const QList<float> &samples);

private:
    void refreshDevices();
    QAudioDevice chosenDevice() const;
    void setState(const QString &state);
    void writeSidecar() const;
    void onStopped(qint64 elapsedMs);

    QThread m_thread;
    RecorderWorker *m_worker = nullptr;
    QMediaDevices m_mediaDevices;
    QVariantList m_devices;
    QString m_deviceId;
    QString m_state = QStringLiteral("idle");
    QString m_path;
    QString m_title;
    QDateTime m_created;
    qint64 m_elapsed = 0;
    QList<float> m_bars;
    QList<qint64> m_markers;
    bool m_silent = false;
    bool m_stopping = false;
};
