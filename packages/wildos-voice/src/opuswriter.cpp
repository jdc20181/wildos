#include "opuswriter.h"

#include <QCoreApplication>

#include <opus/opusenc.h>

#include <unistd.h>

OpusWriter::~OpusWriter()
{
    close();
}

bool OpusWriter::open(const QString &path, int rate, const QString &title, QString *error)
{
    close();
    m_failed = false;
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QCoreApplication::translate("OpusWriter", "Voice couldn't make a file in your Recordings folder.");
        }
        return false;
    }
    OpusEncCallbacks callbacks{&OpusWriter::writePage, &OpusWriter::closeFile};
    OggOpusComments *comments = ope_comments_create();
    ope_comments_add(comments, "TITLE", title.toUtf8().constData());
    ope_comments_add(comments, "ENCODER", "Wild OS Voice");
    int status = OPE_OK;
    m_encoder = ope_encoder_create_callbacks(&callbacks, this, comments, rate, 1, 0, &status);
    ope_comments_destroy(comments);
    if (!m_encoder) {
        m_file.close();
        m_file.remove();
        if (error) {
            *error = QCoreApplication::translate("OpusWriter", "Voice couldn't start the recording (%1).").arg(QString::fromUtf8(ope_strerror(status)));
        }
        return false;
    }
    // Speech at 32 kbit/s is clear and comes to about 14 MB an hour.
    ope_encoder_ctl(m_encoder, OPUS_SET_BITRATE(32000));
    ope_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    // A page at least every half second, so little is lost if the PC dies.
    ope_encoder_ctl(m_encoder, OPE_SET_MUXING_DELAY(24000));
    // The headers now, so even a recording stopped by a crash in its first
    // second is a file players recognise.
    ope_encoder_flush_header(m_encoder);
    m_sinceSync.start();
    return true;
}

bool OpusWriter::write(const float *mono, int frames)
{
    if (!m_encoder || m_failed) {
        return false;
    }
    return ope_encoder_write_float(m_encoder, mono, frames) == OPE_OK && !m_failed;
}

void OpusWriter::close()
{
    if (!m_encoder) {
        return;
    }
    ope_encoder_drain(m_encoder);
    ope_encoder_destroy(m_encoder);
    m_encoder = nullptr;
    if (m_file.isOpen()) {
        m_file.flush();
        ::fdatasync(m_file.handle());
        m_file.close();
    }
}

int OpusWriter::writePage(void *user, const unsigned char *data, int length)
{
    auto *self = static_cast<OpusWriter *>(user);
    if (self->m_file.write(reinterpret_cast<const char *>(data), length) != length || !self->m_file.flush()) {
        self->m_failed = true;
        return 1;
    }
    // Into the kernel at every page, onto the disk every few seconds: often
    // enough for a power cut, rarely enough to leave the disk idle.
    if (self->m_sinceSync.elapsed() > 5000) {
        ::fdatasync(self->m_file.handle());
        self->m_sinceSync.restart();
    }
    return 0;
}

int OpusWriter::closeFile(void *)
{
    // The file is closed in close(), after the last page.
    return 0;
}
