#include "transcriber.h"

#include "audio.h"
#include "modeldownload.h"
#include "paths.h"

#include <QFile>
#include <QRegularExpression>
#include <QThread>

#include <whisper.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr size_t rate = Audio::speechRate;

// Whisper's own messages would only fill the journal.
void quiet(ggml_log_level, const char *, void *)
{
}

bool stopNow(void *user)
{
    auto *worker = static_cast<TranscriberWorker *>(user);
    return worker->quitting.load() || worker->abortFile.load();
}

QString clock(qint64 ms)
{
    const qint64 total = std::max<qint64>(0, ms / 1000);
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    const QString mmss = QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return h > 0 ? QStringLiteral("%1:%2").arg(h).arg(mmss) : mmss;
}

// What Whisper writes for sounds rather than words, "[BLANK_AUDIO]",
// "(music)" or "*laughs*", which a transcript is better without.
bool isNoise(const QString &text)
{
    static const QRegularExpression bracketed(QStringLiteral("^[\\[\\(\\*\\x{266A}].*[\\]\\)\\*\\x{266A}]$"));
    return text.isEmpty() || bracketed.match(text).hasMatch();
}

float loudest(const float *samples, size_t count)
{
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peak = std::max(peak, std::fabs(samples[i]));
    }
    return peak;
}
}

TranscriberWorker::TranscriberWorker(QObject *parent)
    : QObject(parent)
{
    // One core is left for the recording and the window.
    m_threads = std::clamp(QThread::idealThreadCount() - 1, 1, 8);
    whisper_log_set(quiet, nullptr);
}

TranscriberWorker::~TranscriberWorker()
{
    unloadModel();
}

void TranscriberWorker::unloadModel()
{
    if (m_context) {
        whisper_free(m_context);
        m_context = nullptr;
    }
}

bool TranscriberWorker::loadModel()
{
    if (m_context) {
        return true;
    }
    const QString model = SpeechModel::path();
    // Checked again at every start, in case the file was changed or cut
    // short since it was downloaded.
    if (!m_verified) {
        if (!SpeechModel::verify(model)) {
            if (QFile::remove(model)) {
                Q_EMIT modelDamaged();
            }
            return false;
        }
        m_verified = true;
    }
    whisper_context_params params = whisper_context_default_params();
    params.use_gpu = false;
    params.flash_attn = false;
    m_context = whisper_init_from_file_with_params(QFile::encodeName(model).constData(), params);
    if (!m_context) {
        Q_EMIT failed(tr("The speech model couldn't be loaded."));
        return false;
    }
    return true;
}

void TranscriberWorker::begin(const QString &path, qint64 offsetMs, bool live)
{
    if (m_open) {
        finishSession(false);
    }
    if (live) {
        abortFile = false;
    }
    m_path = path;
    m_live = live;
    m_offsetMs = offsetMs;
    m_done = 0;
    m_buffer.clear();
    m_language.clear();
    m_out.setFileName(VoicePaths::transcriptOf(path));
    m_open = m_out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    if (!m_open) {
        Q_EMIT failed(tr("Voice couldn't save the transcript beside the recording."));
        Q_EMIT done(path, false);
    }
}

void TranscriberWorker::feed(const QList<float> &samples)
{
    if (!m_open || quitting.load()) {
        return;
    }
    m_buffer.insert(m_buffer.end(), samples.begin(), samples.end());
    const size_t longest = (m_live ? 15 : 28) * rate;
    const size_t shortest = (m_live ? 8 : 20) * rate;
    while (m_open && m_buffer.size() >= longest && !quitting.load()) {
        process(quietestCut(shortest, longest));
    }
}

void TranscriberWorker::end()
{
    if (!m_open) {
        return;
    }
    while (m_open && !m_buffer.empty() && !quitting.load()) {
        process(std::min(m_buffer.size(), size_t(28 * rate)));
    }
    finishSession(!quitting.load());
}

void TranscriberWorker::finishSession(bool complete)
{
    if (!m_open) {
        return;
    }
    m_out.close();
    m_open = false;
    m_buffer.clear();
    const QString path = m_path;
    // A transcript that never got a line would only claim one exists.
    if (m_out.size() == 0) {
        m_out.remove();
    }
    Q_EMIT done(path, complete);
}

size_t TranscriberWorker::quietestCut(size_t from, size_t to) const
{
    const size_t window = rate / 5;
    const size_t step = rate / 20;
    size_t best = to;
    double bestEnergy = -1;
    for (size_t at = from; at + window <= to && at + window <= m_buffer.size(); at += step) {
        double energy = 0;
        for (size_t i = at; i < at + window; ++i) {
            energy += double(m_buffer[i]) * m_buffer[i];
        }
        if (bestEnergy < 0 || energy < bestEnergy) {
            bestEnergy = energy;
            best = at + window / 2;
        }
    }
    return std::min(best, m_buffer.size());
}

void TranscriberWorker::process(size_t count)
{
    count = std::min(count, m_buffer.size());
    const qint64 startMs = m_offsetMs + m_done * 1000 / qint64(rate);
    // Silence costs nothing and can't be misheard as words. Pieces shorter
    // than a third of a second are too short to say anything.
    const bool worthIt = count >= rate / 3 && loudest(m_buffer.data(), count) > 0.01f;
    if (worthIt && loadModel()) {
        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.n_threads = m_threads;
        params.translate = false;
        params.no_context = true;
        params.single_segment = false;
        params.print_special = false;
        params.print_progress = false;
        params.print_realtime = false;
        params.print_timestamps = false;
        params.suppress_blank = true;
        params.suppress_nst = true;
        // The first piece with speech decides the language; the rest of the
        // recording keeps it, so one unclear sentence can't switch it.
        params.language = m_language.isEmpty() ? "auto" : m_language.constData();
        params.abort_callback = stopNow;
        params.abort_callback_user_data = this;
        if (whisper_full(m_context, params, m_buffer.data(), int(count)) == 0) {
            const int segments = whisper_full_n_segments(m_context);
            for (int i = 0; i < segments; ++i) {
                const QString text = QString::fromUtf8(whisper_full_get_segment_text(m_context, i)).trimmed();
                if (isNoise(text)) {
                    continue;
                }
                const qint64 ms = startMs + whisper_full_get_segment_t0(m_context, i) * 10;
                m_out.write(QStringLiteral("%1 %2\n").arg(clock(ms), text).toUtf8());
                m_out.flush();
                Q_EMIT line(m_path, ms, text);
                if (m_language.isEmpty()) {
                    const int id = whisper_full_lang_id(m_context);
                    if (id >= 0) {
                        m_language = whisper_lang_str(id);
                    }
                }
            }
        }
    }
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + qsizetype(count));
    m_done += qint64(count);
}

void TranscriberWorker::transcribeFile(const QString &path)
{
    abortFile = false;
    if (!loadModel()) {
        Q_EMIT done(path, false);
        return;
    }
    begin(path, 0, false);
    if (!m_open) {
        return;
    }
    const qint64 length = std::max<qint64>(1, Audio::opusDuration(path));
    qint64 fed = 0;
    QString error;
    const bool read = Audio::decodeOpus(
        path,
        [&](const float *samples, int count) {
            feed(QList<float>(samples, samples + count));
            fed += count;
            Q_EMIT progress(path, std::min(1.0, double(fed) * 1000.0 / double(rate) / double(length)));
            return !abortFile.load() && !quitting.load();
        },
        nullptr, nullptr, &error);
    if (!read || abortFile.load() || quitting.load()) {
        // Half a transcript would look like a whole one later.
        m_out.close();
        m_out.remove();
        m_open = false;
        m_buffer.clear();
        if (!error.isEmpty()) {
            Q_EMIT failed(error);
        }
        Q_EMIT done(path, false);
        return;
    }
    end();
}
