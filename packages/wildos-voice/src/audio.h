#pragma once

#include <QList>
#include <QString>

#include <functional>
#include <vector>

// The sound handling shared by recording, the waveforms and transcripts.
namespace Audio
{
// A waveform bar covers this much sound, while recording and after: long
// enough that the live waveform shows the last 15 seconds or so, short
// enough that words still show as separate bars.
constexpr int barMs = 200;
// Whisper listens at 16 kHz, mono.
constexpr int speechRate = 16000;

// A bar's height from 0 to 1 for the loudness (root mean square) of the
// sound in it. On a decibel scale, so quiet speech still moves the bars and
// a shout does not flatten everything else.
float barLevel(double sumOfSquares, int count);

// Turns sound at any rate into 16 kHz for Whisper. Each output sample is
// the average of the input it covers, which keeps the hiss above 8 kHz
// from folding back into the speech.
class Downsampler
{
public:
    explicit Downsampler(int inputRate);
    void push(const float *mono, int frames, std::vector<float> &out);

private:
    double m_step;
    double m_pos = 0;
    double m_sum = 0;
    int m_count = 0;
};

// How long an Ogg Opus file plays, in milliseconds; -1 if it cannot be read.
// A file cut short by a crash still reads up to its last whole page.
qint64 opusDuration(const QString &path);

// Decodes a recording, handing `sink` 16 kHz mono sound a second or so at a
// time; `sink` returns false to stop. Also gives the waveform bars.
bool decodeOpus(const QString &path, const std::function<bool(const float *, int)> &sink, QList<float> *bars, qint64 *durationMs, QString *error);
}
