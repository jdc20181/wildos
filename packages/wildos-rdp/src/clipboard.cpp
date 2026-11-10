#include "clipboard.h"

#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QtEndian>

#include <cstring>

namespace
{
// Windows' standard clipboard formats.
constexpr quint32 cfUnicodeText = 13;
constexpr quint32 cfDib = 8;
// Nothing larger is taken from either side: a picture this size is
// already well past anything worth pasting.
constexpr qsizetype maxBytes = 256 * 1024 * 1024;

QString fromWindowsText(const QByteArray &data)
{
    const auto *units = reinterpret_cast<const char16_t *>(data.constData());
    qsizetype n = data.size() / 2;
    for (qsizetype i = 0; i < n; ++i) {
        if (units[i] == 0) {
            n = i;
            break;
        }
    }
    QString text = QString::fromUtf16(units, n);
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    return text;
}

QByteArray toWindowsText(QString text)
{
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(u'\n', QLatin1String("\r\n"));
    QByteArray out(reinterpret_cast<const char *>(text.utf16()), text.size() * 2);
    out.append(2, '\0');
    return out;
}

// A device-independent bitmap as Windows keeps it on the clipboard is a
// BMP file without its first 14 bytes.
QImage fromDib(const QByteArray &dib)
{
    if (dib.size() < 40) {
        return {};
    }
    const auto *h = reinterpret_cast<const uchar *>(dib.constData());
    const quint32 headerSize = qFromLittleEndian<quint32>(h);
    const quint16 bitCount = qFromLittleEndian<quint16>(h + 14);
    const quint32 compression = qFromLittleEndian<quint32>(h + 16);
    const quint32 coloursUsed = qFromLittleEndian<quint32>(h + 32);
    quint32 masks = 0;
    if (headerSize == 40 && compression == 3) {
        masks = 12;
    } else if (headerSize == 40 && compression == 6) {
        masks = 16;
    }
    const quint32 palette = coloursUsed ? coloursUsed * 4 : (bitCount <= 8 ? (1u << bitCount) * 4 : 0);
    QByteArray file(14, '\0');
    file[0] = 'B';
    file[1] = 'M';
    qToLittleEndian<quint32>(quint32(14 + dib.size()), file.data() + 2);
    qToLittleEndian<quint32>(14 + headerSize + masks + palette, file.data() + 10);
    file.append(dib);
    return QImage::fromData(file, "BMP");
}

QByteArray toDib(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();
    QByteArray dib(40 + qsizetype(w) * h * 4, '\0');
    auto *p = reinterpret_cast<uchar *>(dib.data());
    qToLittleEndian<quint32>(40, p);
    qToLittleEndian<qint32>(w, p + 4);
    qToLittleEndian<qint32>(h, p + 8);
    qToLittleEndian<quint16>(1, p + 12);
    qToLittleEndian<quint16>(32, p + 14);
    qToLittleEndian<quint32>(quint32(w) * h * 4, p + 20);
    uchar *pixels = p + 40;
    for (int y = 0; y < h; ++y) {
        // Bottom-up rows; ARGB32 in memory is B, G, R, A, as Windows has it.
        std::memcpy(pixels + qsizetype(h - 1 - y) * w * 4, image.constScanLine(y), size_t(w) * 4);
    }
    return dib;
}
}

Clipboard::Clipboard(QObject *parent)
    : QObject(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
        if (!m_settingOwn) {
            announce();
        }
    });
}

void Clipboard::attach(CliprdrClientContext *context)
{
    std::lock_guard lock(m_lock);
    m_context = context;
    m_ready = false;
    context->custom = this;
    context->MonitorReady = monitorReady;
    context->ServerCapabilities = serverCapabilities;
    context->ServerFormatList = serverFormatList;
    context->ServerFormatListResponse = serverFormatListResponse;
    context->ServerFormatDataRequest = serverDataRequest;
    context->ServerFormatDataResponse = serverDataResponse;
}

void Clipboard::detach(CliprdrClientContext *context)
{
    std::lock_guard lock(m_lock);
    if (m_context == context) {
        m_context = nullptr;
        m_ready = false;
    }
    context->custom = nullptr;
}

UINT Clipboard::monitorReady(CliprdrClientContext *context, const CLIPRDR_MONITOR_READY *)
{
    auto *self = static_cast<Clipboard *>(context->custom);
    CLIPRDR_GENERAL_CAPABILITY_SET general = {};
    general.capabilitySetType = CB_CAPSTYPE_GENERAL;
    general.capabilitySetLength = CB_CAPSTYPE_GENERAL_LEN;
    general.version = CB_CAPS_VERSION_2;
    general.generalFlags = CB_USE_LONG_FORMAT_NAMES;
    CLIPRDR_CAPABILITIES caps = {};
    caps.cCapabilitiesSets = 1;
    caps.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET *>(&general);
    const UINT rc = context->ClientCapabilities(context, &caps);
    if (rc != CHANNEL_RC_OK) {
        return rc;
    }
    {
        std::lock_guard lock(self->m_lock);
        self->m_ready = true;
    }
    // Whatever the desktop's clipboard holds already can be pasted in
    // Windows straight away.
    QMetaObject::invokeMethod(self, &Clipboard::announce, Qt::QueuedConnection);
    return CHANNEL_RC_OK;
}

UINT Clipboard::serverCapabilities(CliprdrClientContext *, const CLIPRDR_CAPABILITIES *)
{
    return CHANNEL_RC_OK;
}

UINT Clipboard::serverFormatList(CliprdrClientContext *context, const CLIPRDR_FORMAT_LIST *list)
{
    auto *self = static_cast<Clipboard *>(context->custom);
    bool text = false;
    bool image = false;
    for (UINT32 i = 0; i < list->numFormats; ++i) {
        text |= list->formats[i].formatId == cfUnicodeText;
        image |= list->formats[i].formatId == cfDib;
    }
    CLIPRDR_FORMAT_LIST_RESPONSE response = {};
    response.common.msgFlags = CB_RESPONSE_OK;
    const UINT rc = context->ClientFormatListResponse(context, &response);
    if (qEnvironmentVariableIsSet("WILDOS_RDP_DEBUG")) {
        qInfo("clipboard: Windows offers text %d, picture %d", text, image);
    }
    QMetaObject::invokeMethod(
        self,
        [self, text, image] {
            self->m_offerText = text;
            self->m_offerImage = image;
            self->m_text.clear();
            self->m_image.clear();
            self->m_requested = 0;
            self->fetchNext();
        },
        Qt::QueuedConnection);
    return rc;
}

UINT Clipboard::serverFormatListResponse(CliprdrClientContext *, const CLIPRDR_FORMAT_LIST_RESPONSE *)
{
    return CHANNEL_RC_OK;
}

UINT Clipboard::serverDataRequest(CliprdrClientContext *context, const CLIPRDR_FORMAT_DATA_REQUEST *request)
{
    auto *self = static_cast<Clipboard *>(context->custom);
    const quint32 format = request->requestedFormatId;
    QMetaObject::invokeMethod(self, [self, format] { self->answer(format); }, Qt::QueuedConnection);
    return CHANNEL_RC_OK;
}

UINT Clipboard::serverDataResponse(CliprdrClientContext *context, const CLIPRDR_FORMAT_DATA_RESPONSE *response)
{
    auto *self = static_cast<Clipboard *>(context->custom);
    QByteArray data;
    if ((response->common.msgFlags & CB_RESPONSE_OK) && response->requestedFormatData && response->common.dataLen <= maxBytes) {
        data = QByteArray(reinterpret_cast<const char *>(response->requestedFormatData), qsizetype(response->common.dataLen));
    }
    QMetaObject::invokeMethod(self, [self, data] { self->received(self->m_requested, data); }, Qt::QueuedConnection);
    return CHANNEL_RC_OK;
}

void Clipboard::fetchNext()
{
    quint32 format = 0;
    if (m_offerText && m_text.isNull()) {
        format = cfUnicodeText;
    } else if (m_offerImage && m_image.isNull()) {
        format = cfDib;
    }
    if (!format) {
        auto *mime = new QMimeData;
        if (!m_text.isEmpty()) {
            mime->setText(fromWindowsText(m_text));
        }
        if (!m_image.isEmpty()) {
            const QImage image = fromDib(m_image);
            if (!image.isNull()) {
                mime->setImageData(image);
            }
        }
        if (mime->formats().isEmpty()) {
            delete mime;
            return;
        }
        m_settingOwn = true;
        QGuiApplication::clipboard()->setMimeData(mime);
        m_settingOwn = false;
        if (qEnvironmentVariableIsSet("WILDOS_RDP_DEBUG")) {
            qInfo("clipboard: the desktop's clipboard now holds Windows' copy");
        }
        return;
    }
    std::lock_guard lock(m_lock);
    if (!m_context || !m_ready) {
        return;
    }
    m_requested = format;
    CLIPRDR_FORMAT_DATA_REQUEST request = {};
    request.requestedFormatId = format;
    if (m_context->ClientFormatDataRequest(m_context, &request) != CHANNEL_RC_OK) {
        m_requested = 0;
    }
}

void Clipboard::received(quint32 format, const QByteArray &data)
{
    if (qEnvironmentVariableIsSet("WILDOS_RDP_DEBUG")) {
        qInfo("clipboard: format %u, %lld bytes from Windows", format, qint64(data.size()));
    }
    // A format Windows could not give is marked done with an empty array,
    // which is not a null one.
    const QByteArray value = data.isNull() ? QByteArray("") : data;
    if (format == cfUnicodeText) {
        m_text = value;
    } else if (format == cfDib) {
        m_image = value;
    } else {
        return;
    }
    fetchNext();
}

void Clipboard::announce()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QList<CLIPRDR_FORMAT> formats;
    if (mime && mime->hasText()) {
        formats.append({cfUnicodeText, nullptr});
    }
    if (mime && mime->hasImage()) {
        formats.append({cfDib, nullptr});
    }
    std::lock_guard lock(m_lock);
    if (!m_context || !m_ready) {
        return;
    }
    CLIPRDR_FORMAT_LIST list = {};
    list.common.msgType = CB_FORMAT_LIST;
    list.numFormats = UINT32(formats.size());
    list.formats = formats.data();
    (void)m_context->ClientFormatList(m_context, &list);
}

void Clipboard::answer(quint32 format)
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QByteArray data;
    if (mime && format == cfUnicodeText && mime->hasText()) {
        data = toWindowsText(mime->text());
    } else if (mime && format == cfDib && mime->hasImage()) {
        data = toDib(qvariant_cast<QImage>(mime->imageData()));
    }
    std::lock_guard lock(m_lock);
    if (!m_context) {
        return;
    }
    CLIPRDR_FORMAT_DATA_RESPONSE response = {};
    response.common.msgType = CB_FORMAT_DATA_RESPONSE;
    response.common.msgFlags = data.isEmpty() || data.size() > maxBytes ? CB_RESPONSE_FAIL : CB_RESPONSE_OK;
    response.common.dataLen = response.common.msgFlags == CB_RESPONSE_OK ? UINT32(data.size()) : 0;
    response.requestedFormatData = reinterpret_cast<const BYTE *>(data.constData());
    (void)m_context->ClientFormatDataResponse(m_context, &response);
}
