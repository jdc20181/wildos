#include "transcripts.h"

#include "paths.h"
#include "recorder.h"
#include "recordings.h"
#include "transcriber.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

namespace
{
Transcripts *s_instance = nullptr;

bool processorCanRun()
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma") && __builtin_cpu_supports("f16c");
#else
    return true;
#endif
}

// "01:31 Fiona has said yes" or "1:02:03 ...", as TranscriberWorker writes.
QVariantList readTranscript(const QString &opusPath)
{
    static const QRegularExpression timed(QStringLiteral("^(?:(\\d+):)?(\\d{1,2}):(\\d{2}) (.*)$"));
    QVariantList lines;
    QFile file(VoicePaths::transcriptOf(opusPath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.size() > 8 * 1024 * 1024) {
        return lines;
    }
    while (!file.atEnd()) {
        const QString text = QString::fromUtf8(file.readLine()).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch match = timed.match(text);
        if (match.hasMatch()) {
            const qint64 ms = (match.captured(1).toLongLong() * 3600 + match.captured(2).toLongLong() * 60 + match.captured(3).toLongLong()) * 1000;
            lines.append(QVariantMap{{QStringLiteral("time"), double(ms)}, {QStringLiteral("text"), match.captured(4)}});
        } else {
            // A line someone typed in by hand.
            lines.append(QVariantMap{{QStringLiteral("time"), -1.0}, {QStringLiteral("text"), text}});
        }
    }
    return lines;
}
}

Transcripts::Transcripts(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_supported = processorCanRun();
    m_modelReady = QFileInfo(SpeechModel::path()).size() == SpeechModel::size;
    m_enabled = m_supported && m_modelReady && QSettings().value(QStringLiteral("Transcripts/enabled"), false).toBool();

    m_thread.setObjectName(QStringLiteral("Transcripts"));
    m_worker = new TranscriberWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &TranscriberWorker::line, this, [this](const QString &path, qint64 ms, const QString &text) {
        if (path == m_shownPath) {
            m_lines.append(QVariantMap{{QStringLiteral("time"), double(ms)}, {QStringLiteral("text"), text}});
            Q_EMIT linesChanged();
        }
    });
    connect(m_worker, &TranscriberWorker::progress, this, [this](const QString &path, double fraction) {
        if (path == m_workingPath) {
            setWorking(path, fraction);
        }
    });
    connect(m_worker, &TranscriberWorker::done, this, [this](const QString &path, bool) {
        if (path == m_workingPath && path != m_livePath) {
            setWorking(QString(), -1);
        }
        if (path == m_shownPath) {
            reloadLines();
        }
        Recordings::instance()->refresh();
    });
    connect(m_worker, &TranscriberWorker::failed, this, &Transcripts::setMessage);
    connect(m_worker, &TranscriberWorker::modelDamaged, this, [this]() {
        m_modelReady = false;
        Q_EMIT modelChanged();
        endLive();
        setWorking(QString(), -1);
        m_enabled = false;
        QSettings().setValue(QStringLiteral("Transcripts/enabled"), false);
        Q_EMIT enabledChanged();
        setMessage(tr("The speech model on this PC was damaged, so it was deleted. Turn transcripts on to download it again."));
    });
    // Below the recording and the window, so writing down never makes
    // either stutter.
    m_thread.start(QThread::LowPriority);

    connect(&m_download, &ModelDownload::progress, this, [this](qint64 received, qint64 total) {
        m_downloadProgress = total > 0 ? double(received) / double(total) : 0;
        Q_EMIT downloadChanged();
    });
    connect(&m_download, &ModelDownload::finished, this, [this](bool ok, const QString &message) {
        Q_EMIT downloadChanged();
        if (!ok) {
            m_enabled = false;
            Q_EMIT enabledChanged();
            setMessage(message);
            return;
        }
        m_modelReady = true;
        Q_EMIT modelChanged();
        QSettings().setValue(QStringLiteral("Transcripts/enabled"), true);
        Recorder *recorder = Recorder::instance();
        if (m_enabled && recorder->active()) {
            beginLive(recorder->path(), qint64(recorder->elapsed()));
        }
    });
}

Transcripts::~Transcripts()
{
    shutDown();
}

void Transcripts::shutDown()
{
    if (!m_thread.isRunning()) {
        return;
    }
    m_worker->quitting = true;
    m_download.cancel();
    m_thread.quit();
    m_thread.wait();
}

Transcripts *Transcripts::instance()
{
    if (!s_instance) {
        new Transcripts(qApp);
    }
    return s_instance;
}

Transcripts *Transcripts::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void Transcripts::setEnabled(bool on)
{
    if (on == m_enabled) {
        return;
    }
    clearMessage();
    if (on && !m_supported) {
        setMessage(tr("This PC's processor is too old for the speech model, so it can't make transcripts."));
        Q_EMIT enabledChanged();
        return;
    }
    m_enabled = on;
    Q_EMIT enabledChanged();
    if (!on) {
        m_download.cancel();
        endLive();
        QSettings().setValue(QStringLiteral("Transcripts/enabled"), false);
        return;
    }
    if (!m_modelReady) {
        m_downloadProgress = 0;
        m_download.start();
        Q_EMIT downloadChanged();
        return;
    }
    QSettings().setValue(QStringLiteral("Transcripts/enabled"), true);
    Recorder *recorder = Recorder::instance();
    if (recorder->active()) {
        beginLive(recorder->path(), qint64(recorder->elapsed()));
    }
}

void Transcripts::cancelDownload()
{
    if (m_download.running()) {
        setEnabled(false);
    }
}

void Transcripts::clearMessage()
{
    setMessage(QString());
}

void Transcripts::setMessage(const QString &message)
{
    if (message != m_message) {
        m_message = message;
        Q_EMIT messageChanged();
    }
}

void Transcripts::setWorking(const QString &path, double progress)
{
    m_workingPath = path;
    m_fileProgress = progress;
    Q_EMIT workingChanged();
}

void Transcripts::setShownPath(const QString &path)
{
    if (path == m_shownPath) {
        return;
    }
    m_shownPath = path;
    reloadLines();
}

void Transcripts::reloadLines()
{
    m_lines = m_shownPath.isEmpty() ? QVariantList() : readTranscript(m_shownPath);
    Q_EMIT linesChanged();
}

void Transcripts::transcribe(const QString &path)
{
    if (!m_enabled || !m_modelReady || !m_workingPath.isEmpty() || path.isEmpty()) {
        return;
    }
    clearMessage();
    setWorking(path, 0);
    if (path == m_shownPath) {
        m_lines.clear();
        Q_EMIT linesChanged();
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, path]() {
        worker->transcribeFile(path);
    });
}

void Transcripts::beginLive(const QString &path, qint64 offsetMs)
{
    if (!m_livePath.isEmpty() || path.isEmpty()) {
        return;
    }
    // A recording comes first: a transcript of an older one stops, and
    // can be made again later.
    if (!m_workingPath.isEmpty()) {
        m_worker->abortFile = true;
    }
    m_livePath = path;
    setWorking(path, -1);
    if (path == m_shownPath) {
        m_lines.clear();
        Q_EMIT linesChanged();
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, path, offsetMs]() {
        worker->begin(path, offsetMs, true);
    });
}

void Transcripts::endLive()
{
    if (m_livePath.isEmpty()) {
        return;
    }
    m_livePath.clear();
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->end();
    });
}

void Transcripts::recordingStarted(const QString &path)
{
    if (m_enabled && m_modelReady) {
        beginLive(path, 0);
    }
}

void Transcripts::recordingFinished(const QString &path)
{
    Q_UNUSED(path)
    endLive();
}

void Transcripts::speech(const QList<float> &samples)
{
    if (m_livePath.isEmpty()) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, samples]() {
        worker->feed(samples);
    });
}
