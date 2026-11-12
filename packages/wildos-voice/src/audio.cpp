#include "audio.h"

#include <QCoreApplication>
#include <QFile>

#include <opusfile.h>

#include <algorithm>
#include <cmath>

namespace Audio
{
float barLevel(double sumOfSquares, int count)
{
    if (count <= 0 || sumOfSquares <= 0) {
        return 0.0f;
    }
    // -50 dB and below is the floor; loud speech close to the microphone,
    // about -8 dB, reaches the top.
    const double db = 10.0 * std::log10(sumOfSquares / count);
    return float(std::clamp((db + 50.0) / 42.0, 0.0, 1.0));
}

Downsampler::Downsampler(int inputRate)
    : m_step(double(speechRate) / double(std::max(1, inputRate)))
{
}

void Downsampler::push(const float *mono, int frames, std::vector<float> &out)
{
    for (int i = 0; i < frames; ++i) {
        m_sum += mono[i];
        ++m_count;
        m_pos += m_step;
        if (m_pos >= 1.0) {
            m_pos -= 1.0;
            out.push_back(float(m_sum / m_count));
            m_sum = 0;
            m_count = 0;
        }
    }
}

namespace
{
OggOpusFile *openOpus(const QString &path)
{
    int error = 0;
    return op_open_file(QFile::encodeName(path).constData(), &error);
}
}

qint64 opusDuration(const QString &path)
{
    OggOpusFile *file = openOpus(path);
    if (!file) {
        return -1;
    }
    const ogg_int64_t samples = op_pcm_total(file, -1);
    op_free(file);
    return samples < 0 ? -1 : samples / 48;
}

bool decodeOpus(const QString &path, const std::function<bool(const float *, int)> &sink, QList<float> *bars, qint64 *durationMs, QString *error)
{
    OggOpusFile *file = openOpus(path);
    if (!file) {
        if (error) {
            *error = QCoreApplication::translate("Audio", "This recording can't be read.");
        }
        return false;
    }
    // opusfile always decodes at 48 kHz.
    constexpr int rate = 48000;
    constexpr int perBar = rate * barMs / 1000;
    Downsampler down(rate);
    std::vector<float> pcm(rate * 2 * 2);
    std::vector<float> mono;
    std::vector<float> speech;
    double energy = 0;
    int inBar = 0;
    qint64 total = 0;
    bool ok = true;
    for (;;) {
        const int frames = op_read_float_stereo(file, pcm.data(), int(pcm.size()));
        if (frames == OP_HOLE) {
            continue;
        }
        if (frames < 0) {
            // Past the last whole page of a file a crash cut short: keep
            // what was read.
            break;
        }
        if (frames == 0) {
            break;
        }
        mono.resize(frames);
        for (int i = 0; i < frames; ++i) {
            const float s = 0.5f * (pcm[2 * i] + pcm[2 * i + 1]);
            mono[i] = s;
            if (bars) {
                energy += double(s) * s;
                if (++inBar == perBar) {
                    bars->append(barLevel(energy, inBar));
                    energy = 0;
                    inBar = 0;
                }
            }
        }
        total += frames;
        if (sink) {
            speech.clear();
            down.push(mono.data(), frames, speech);
            if (!speech.empty() && !sink(speech.data(), int(speech.size()))) {
                ok = false;
                break;
            }
        }
    }
    if (bars && inBar > perBar / 4) {
        bars->append(barLevel(energy, inBar));
    }
    op_free(file);
    if (durationMs) {
        *durationMs = total * 1000 / rate;
    }
    return ok;
}
}
