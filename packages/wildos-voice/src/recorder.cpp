#include "recorder.h"

#include "paths.h"
#include "recordings.h"
#include "sidecar.h"

#include <QAudioSource>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
Recorder *s_instance = nullptr;

// Enough bars for the widest waveform the window can show.
constexpr int recentCount = 400;
// This long without a single sample above the noise floor means the
// microphone is muted or switched off.
constexpr int silentSeconds = 3;

// Settings > Sound's names where the device says what it is; anything
// else keeps the name the sound system gives it.
QString friendlyName(const QAudioDevice &device)
{
    const QString description = device.description();
    if (description.contains(QLatin1String("headset"), Qt::CaseInsensitive)) {
        return QCoreApplication::translate("Recorder", "Headset microphone");
    }
    if (description.contains(QLatin1String("built-in"), Qt::CaseInsensitive) || description.contains(QLatin1String("internal"), Qt::CaseInsensitive)) {
        return QCoreApplication::translate("Recorder", "Built-in microphone");
    }
    return description;
}
}

RecorderWorker::RecorderWorker(QObject *parent)
    : QObject(parent)
    , m_watchdog(this)
{
    m_watchdog.setInterval(500);
    connect(&m_watchdog, &QTimer::timeout, this, [this]() {
        if (!m_source || m_paused || m_sinceData.elapsed() < 3000) {
            return;
        }
        finish(m_frames == 0 ? tr("Voice couldn't open the microphone. Check Settings > Sound.")
                             : tr("The microphone stopped. What was recorded is saved."));
    });
}

RecorderWorker::~RecorderWorker()
{
    if (m_source) {
        m_source->stop();
    }
    m_writer.close();
}

void RecorderWorker::start(const QAudioDevice &device, const QString &path, const QString &title)
{
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format)) {
        format.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(format)) {
            format = device.preferredFormat();
        }
    }
    m_format = format;
    m_frames = 0;
    if (!m_format.isValid()) {
        finish(tr("Voice couldn't open the microphone. Check Settings > Sound."));
        return;
    }
    QString error;
    if (!m_writer.open(path, m_format.sampleRate(), title, &error)) {
        finish(error);
        return;
    }
    m_down = std::make_unique<Audio::Downsampler>(m_format.sampleRate());
    m_perBar = std::max(1, m_format.sampleRate() * Audio::barMs / 1000);
    m_inBar = 0;
    m_energy = 0;
    m_quietFrames = 0;
    m_silent = false;
    m_paused = false;
    m_pendingBars.clear();
    m_speech.clear();

    m_source = new QAudioSource(device, m_format, this);
    // Small reads keep the waveform moving smoothly.
    m_source->setBufferSize(m_format.bytesForDuration(200000));
    m_io = m_source->start();
    if (!m_io || m_source->error() != QAudio::NoError) {
        finish(tr("Voice couldn't open the microphone. Check Settings > Sound."));
        return;
    }
    connect(m_io, &QIODevice::readyRead, this, &RecorderWorker::readAvailable);
    m_sinceData.start();
    m_watchdog.start();
    connect(m_source, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && m_source && m_source->error() != QAudio::NoError) {
            finish(tr("The microphone stopped. What was recorded is saved."));
        }
    });
    Q_EMIT started();
}

void RecorderWorker::setPaused(bool paused)
{
    if (!m_source || paused == m_paused) {
        return;
    }
    // Read what came in before the pause, then drop anything that arrives
    // until the resume: a paused recording just leaves that part out.
    readAvailable();
    m_paused = paused;
    if (paused) {
        m_source->suspend();
    } else {
        m_source->resume();
        m_sinceData.restart();
    }
}

void RecorderWorker::stop()
{
    if (!m_source) {
        return;
    }
    if (!m_paused) {
        readAvailable();
    }
    finish(QString());
}

void RecorderWorker::finish(const QString &failure)
{
    m_watchdog.stop();
    if (m_source) {
        m_source->disconnect(this);
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
        m_io = nullptr;
    }
    if (!m_speech.empty()) {
        Q_EMIT speech(QList<float>(m_speech.begin(), m_speech.end()));
        m_speech.clear();
    }
    if (!m_pendingBars.isEmpty()) {
        Q_EMIT bars(m_pendingBars, m_frames * 1000 / std::max(1, m_format.sampleRate()));
        m_pendingBars.clear();
    }
    m_writer.close();
    if (!failure.isEmpty()) {
        Q_EMIT failed(failure);
    }
    Q_EMIT stopped(m_frames * 1000 / std::max(1, m_format.sampleRate()));
}

void RecorderWorker::readAvailable()
{
    if (!m_io) {
        return;
    }
    const QByteArray data = m_io->readAll();
    if (data.isEmpty() || m_paused) {
        return;
    }
    m_sinceData.restart();
    const int channels = std::max(1, m_format.channelCount());
    const int bytesPerFrame = m_format.bytesPerFrame();
    if (bytesPerFrame <= 0) {
        return;
    }
    const int frames = int(data.size() / bytesPerFrame);
    m_mono.resize(frames);
    const char *raw = data.constData();
    for (int i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += m_format.normalizedSampleValue(raw + (i * channels + c) * m_format.bytesPerSample());
        }
        m_mono[i] = sum / float(channels);
    }
    consume(m_mono.data(), frames);
}

void RecorderWorker::consume(const float *mono, int frames)
{
    if (frames <= 0) {
        return;
    }
    if (!m_writer.write(mono, frames)) {
        finish(tr("The disk is full, so recording stopped. What was recorded is saved."));
        return;
    }
    bool heard = false;
    for (int i = 0; i < frames; ++i) {
        const float level = std::fabs(mono[i]);
        m_energy += double(mono[i]) * mono[i];
        heard = heard || level > 1.0f / 32768.0f;
        if (++m_inBar == m_perBar) {
            m_pendingBars.append(Audio::barLevel(m_energy, m_inBar));
            m_energy = 0;
            m_inBar = 0;
        }
    }
    m_frames += frames;
    m_quietFrames = heard ? 0 : m_quietFrames + frames;
    const bool silent = m_quietFrames > qint64(silentSeconds) * m_format.sampleRate();
    if (silent != m_silent) {
        m_silent = silent;
        Q_EMIT silentChanged(silent);
    }
    if (!m_pendingBars.isEmpty()) {
        Q_EMIT bars(m_pendingBars, m_frames * 1000 / m_format.sampleRate());
        m_pendingBars.clear();
    }
    m_down->push(mono, frames, m_speech);
    if (m_speech.size() >= size_t(Audio::speechRate)) {
        Q_EMIT speech(QList<float>(m_speech.begin(), m_speech.end()));
        m_speech.clear();
    }
}

Recorder::Recorder(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_deviceId = QSettings().value(QStringLiteral("Recording/device")).toString();
    m_thread.setObjectName(QStringLiteral("Recording"));
    m_worker = new RecorderWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &RecorderWorker::bars, this, [this](const QList<float> &levels, qint64 elapsedMs) {
        m_bars.append(levels);
        m_elapsed = elapsedMs;
        Q_EMIT elapsedChanged();
    });
    connect(m_worker, &RecorderWorker::speech, this, &Recorder::speech);
    connect(m_worker, &RecorderWorker::silentChanged, this, [this](bool silent) {
        m_silent = silent;
        Q_EMIT silentChanged();
    });
    connect(m_worker, &RecorderWorker::failed, this, &Recorder::failed);
    connect(m_worker, &RecorderWorker::stopped, this, &Recorder::onStopped);
    // Audio is read every few milliseconds; the thread gets a head start
    // over the window so a busy moment there never drops sound.
    m_thread.start(QThread::HighPriority);

    connect(&m_mediaDevices, &QMediaDevices::audioInputsChanged, this, &Recorder::refreshDevices);
    refreshDevices();
}

Recorder::~Recorder()
{
    m_thread.quit();
    m_thread.wait();
}

Recorder *Recorder::instance()
{
    if (!s_instance) {
        new Recorder(qApp);
    }
    return s_instance;
}

Recorder *Recorder::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void Recorder::refreshDevices()
{
    QVariantList devices;
    QStringList names;
    QList<QAudioDevice> inputs;
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        // What the speakers play, not a microphone.
        if (device.id().endsWith(".monitor")) {
            continue;
        }
        inputs.append(device);
        names.append(friendlyName(device));
    }
    for (int i = 0; i < inputs.size(); ++i) {
        const QString name = names.count(names.at(i)) > 1 ? inputs.at(i).description() : names.at(i);
        devices.append(QVariantMap{{QStringLiteral("id"), QString::fromUtf8(inputs.at(i).id())}, {QStringLiteral("name"), name}});
    }
    m_devices = devices;
    Q_EMIT devicesChanged();
}

QAudioDevice Recorder::chosenDevice() const
{
    if (!m_deviceId.isEmpty()) {
        for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
            if (QString::fromUtf8(device.id()) == m_deviceId) {
                return device;
            }
        }
    }
    const QAudioDevice fallback = QMediaDevices::defaultAudioInput();
    if (!fallback.isNull() && !fallback.id().endsWith(".monitor")) {
        return fallback;
    }
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        if (!device.id().endsWith(".monitor")) {
            return device;
        }
    }
    return {};
}

void Recorder::setDeviceId(const QString &id)
{
    if (id == m_deviceId) {
        return;
    }
    m_deviceId = id;
    QSettings().setValue(QStringLiteral("Recording/device"), id);
    Q_EMIT devicesChanged();
}

QString Recorder::deviceName() const
{
    const QString id = QString::fromUtf8(chosenDevice().id());
    for (const QVariant &device : m_devices) {
        const QVariantMap map = device.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            return map.value(QStringLiteral("name")).toString();
        }
    }
    return tr("No microphone");
}

QVariantList Recorder::recentBars() const
{
    QVariantList bars;
    const qsizetype from = std::max<qsizetype>(0, m_bars.size() - recentCount);
    bars.reserve(m_bars.size() - from);
    for (qsizetype i = from; i < m_bars.size(); ++i) {
        bars.append(m_bars.at(i));
    }
    return bars;
}

QVariantList Recorder::markers() const
{
    QVariantList list;
    for (qint64 marker : m_markers) {
        list.append(double(marker));
    }
    return list;
}

void Recorder::setState(const QString &state)
{
    if (state != m_state) {
        m_state = state;
        Q_EMIT stateChanged();
    }
}

void Recorder::start()
{
    if (active() || m_stopping) {
        return;
    }
    const QAudioDevice device = chosenDevice();
    if (device.isNull()) {
        Q_EMIT failed(tr("No microphone found. Check Settings > Sound."));
        return;
    }
    QString title;
    const QString path = Recordings::instance()->newRecordingPath(&title);
    if (path.isEmpty()) {
        Q_EMIT failed(tr("Voice couldn't make the Recordings folder."));
        return;
    }
    m_path = path;
    m_title = title;
    m_created = QDateTime::currentDateTime();
    m_elapsed = 0;
    m_bars.clear();
    m_markers.clear();
    m_silent = false;
    writeSidecar();
    setState(QStringLiteral("recording"));
    Q_EMIT elapsedChanged();
    Q_EMIT markersChanged();
    Q_EMIT silentChanged();
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, device, path, title]() {
        worker->start(device, path, title);
    });
    Q_EMIT started(path);
}

void Recorder::pause()
{
    if (m_state != QLatin1String("recording")) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->setPaused(true);
    });
    setState(QStringLiteral("paused"));
}

void Recorder::resume()
{
    if (m_state != QLatin1String("paused")) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->setPaused(false);
    });
    setState(QStringLiteral("recording"));
}

void Recorder::stop()
{
    if (!active() || m_stopping) {
        return;
    }
    m_stopping = true;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->stop();
    });
}

void Recorder::stopNow()
{
    if (!active()) {
        return;
    }
    m_stopping = true;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker]() {
            worker->stop();
        },
        Qt::BlockingQueuedConnection);
    // The stopped signal is queued behind the call; take it now.
    QCoreApplication::sendPostedEvents(this);
}

void Recorder::mark()
{
    if (!active()) {
        return;
    }
    if (!m_markers.isEmpty() && m_elapsed - m_markers.last() < 1000) {
        return;
    }
    m_markers.append(m_elapsed);
    writeSidecar();
    Q_EMIT markersChanged();
}

void Recorder::writeSidecar() const
{
    Sidecar sidecar;
    sidecar.title = m_title;
    sidecar.created = m_created;
    sidecar.markers = m_markers;
    if (!active() && !m_path.isEmpty()) {
        sidecar.durationMs = m_elapsed;
    }
    sidecar.write(m_path);
}

void Recorder::onStopped(qint64 elapsedMs)
{
    if (!active()) {
        return;
    }
    m_elapsed = elapsedMs;
    const QString path = m_path;
    m_stopping = false;
    setState(QStringLiteral("idle"));
    if (m_elapsed < 300) {
        // Nothing worth keeping: a Record pressed and stopped at once, or a
        // microphone that never opened.
        QFile::remove(path);
        QFile::remove(VoicePaths::sidecarOf(path));
    } else {
        writeSidecar();
    }
    m_path.clear();
    Q_EMIT stateChanged();
    Recordings::instance()->refresh();
    Q_EMIT finished(QFileInfo::exists(path) ? path : QString());
}
