#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QString>

struct OggOpusEnc;

// Writes mono sound to an Ogg Opus file with libopusenc, a page about
// every half second, each one handed to the disk as it is made. A crash or
// a power cut leaves a file that plays up to the last page or two.
class OpusWriter
{
public:
    OpusWriter() = default;
    ~OpusWriter();
    OpusWriter(const OpusWriter &) = delete;
    OpusWriter &operator=(const OpusWriter &) = delete;

    bool open(const QString &path, int rate, const QString &title, QString *error);
    bool write(const float *mono, int frames);
    // Finishes the stream with its last page; the file is complete after.
    void close();
    bool isOpen() const { return m_encoder != nullptr; }

private:
    static int writePage(void *user, const unsigned char *data, int length);
    static int closeFile(void *user);

    OggOpusEnc *m_encoder = nullptr;
    QFile m_file;
    QElapsedTimer m_sinceSync;
    bool m_failed = false;
};
