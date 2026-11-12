#include "composer.h"
#include "formats.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSettings>
#include <QTime>
#include <QUrl>
#include <QUuid>

#include <camel/camel.h>

#include "mailengine.h"

namespace
{
QString str(const gchar *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

CamelMimePart *textPart(const QString &text, const char *subtype)
{
    CamelMimePart *part = camel_mime_part_new();
    const QByteArray bytes = text.toUtf8();
    const QByteArray type = QByteArray("text/") + subtype + "; charset=utf-8";
    camel_mime_part_set_content(part, bytes.constData(), bytes.size(), type.constData());
    camel_mime_part_set_encoding(part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
    return part;
}

CamelInternetAddress *addresses(const QString &text)
{
    CamelInternetAddress *list = camel_internet_address_new();
    if (!text.trimmed().isEmpty()) {
        camel_address_unformat(CAMEL_ADDRESS(list), text.toUtf8().constData());
    }
    return list;
}

bool validAddresses(const QString &text, QString *bad)
{
    static const QRegularExpression email(QStringLiteral(R"(^[^@\s<>]+@[^@\s<>]+\.[^@\s<>]+$)"));
    CamelInternetAddress *list = addresses(text);
    bool ok = true;
    for (int i = 0; i < camel_address_length(CAMEL_ADDRESS(list)); ++i) {
        const gchar *a = nullptr;
        camel_internet_address_get(list, i, nullptr, &a);
        if (!email.match(str(a)).hasMatch()) {
            *bad = str(a);
            ok = false;
            break;
        }
    }
    g_object_unref(list);
    return ok;
}

// Photos lose everything but the picture: where they were taken, the camera
// and when. The picture is drawn the right way up first, since the rotation
// lives in the same data.
QByteArray cleanPhoto(const QByteArray &data, const QByteArray &format, const QByteArray &readFormat = {})
{
    QBuffer in;
    in.setData(data);
    in.open(QIODevice::ReadOnly);
    QImageReader reader(&in, readFormat.isEmpty() ? format : readFormat);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    writer.setQuality(92);
    return writer.write(image) ? out : QByteArray();
}

QString quoteHtml(const QString &header, const QString &quote)
{
    if (quote.isEmpty()) {
        return {};
    }
    QString body = quote.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    QString head = header.toHtmlEscaped();
    head.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return QStringLiteral("<br><div>%1</div><blockquote style=\"margin:0 0 0 4px;padding-left:12px;border-left:2px solid #c3c8d4;color:#5c5e62\">%2</blockquote>")
        .arg(head, body);
}

QString quotePlain(const QString &header, const QString &quote)
{
    if (quote.isEmpty()) {
        return {};
    }
    QString out = QStringLiteral("\n\n") + header + QLatin1Char('\n');
    for (const QString &line : quote.split(QLatin1Char('\n'))) {
        out += QStringLiteral("> ") + line + QLatin1Char('\n');
    }
    return out;
}
}

Composer::Composer(QObject *parent)
    : QObject(parent)
{
    m_draftTimer.setSingleShot(true);
    m_draftTimer.setInterval(2000);
    connect(&m_draftTimer, &QTimer::timeout, this, &Composer::saveDraft);
}

QVariantList Composer::attachments() const
{
    QVariantList list;
    for (int i = 0; i < m_attachments.size(); ++i) {
        const Attachment &a = m_attachments[i];
        list.append(QVariantMap{{QStringLiteral("name"), a.name},
                                {QStringLiteral("size"), QLocale().formattedDataSize(a.data.size(), 0, QLocale::DataSizeTraditionalFormat)},
                                {QStringLiteral("type"), a.mimeType},
                                {QStringLiteral("photo"), a.photoCleaned},
                                {QStringLiteral("uncleaned"), a.photoUncleaned},
                                {QStringLiteral("index"), i}});
    }
    return list;
}

void Composer::start(const QVariantMap &data)
{
    m_accountId = data.value(QStringLiteral("accountId")).toString();
    if (m_accountId.isEmpty()) {
        const QVariantList accounts = MailEngine::instance()->accountList();
        if (!accounts.isEmpty()) {
            m_accountId = accounts.first().toMap().value(QStringLiteral("id")).toString();
        }
    }
    m_to = data.value(QStringLiteral("to")).toStringList().join(QStringLiteral(", "));
    m_cc = data.value(QStringLiteral("cc")).toStringList().join(QStringLiteral(", "));
    m_bcc.clear();
    m_subject = data.value(QStringLiteral("subject")).toString();
    m_quoteHeader = data.value(QStringLiteral("quoteHeader")).toString();
    m_quote = data.value(QStringLiteral("quote")).toString();
    m_inReplyTo = data.value(QStringLiteral("inReplyTo")).toString();
    m_references = data.value(QStringLiteral("references")).toString();
    m_calendarReply = data.value(QStringLiteral("calendarReply")).toString();
    m_bodyHtml.clear();
    m_bodyText.clear();
    m_attachments.clear();
    m_draftUid.clear();
    m_savedAt.clear();
    m_started = true;
    if (const Account *a = MailEngine::instance()->accountData(m_accountId); a && !a->signature.isEmpty()) {
        m_bodyText = QStringLiteral("\n\n") + a->signature;
    }
    Q_EMIT changed();
    Q_EMIT attachmentsChanged();
    Q_EMIT savedAtChanged();
}

void Composer::addAttachment(const QUrl &url)
{
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT problem(tr("That file couldn't be opened."));
        return;
    }
    if (file.size() > 25 * 1024 * 1024) {
        Q_EMIT problem(tr("That file is over 25 MB, more than most email providers accept."));
        return;
    }
    Attachment a;
    a.name = QFileInfo(file).fileName();
    a.mimeType = QMimeDatabase().mimeTypeForFile(file.fileName()).name();
    a.data = file.readAll();
    QByteArray format;
    if (a.mimeType == QLatin1String("image/jpeg")) {
        format = "jpeg";
    } else if (a.mimeType == QLatin1String("image/png")) {
        format = "png";
    } else if (a.mimeType == QLatin1String("image/webp")) {
        format = "webp";
    }
    if (!format.isEmpty()) {
        const QByteArray clean = cleanPhoto(a.data, format);
        if (!clean.isEmpty()) {
            a.data = clean;
            a.photoCleaned = true;
        }
    } else if (a.mimeType.startsWith(QLatin1String("image/")) && a.mimeType != QLatin1String("image/gif") && a.mimeType != QLatin1String("image/svg+xml")) {
        // Other photo formats (HEIC from iPhones, TIFF, RAW) go as JPEG, the
        // one way to be sure their location goes too.
        const QByteArray clean = cleanPhoto(a.data, "jpeg", QByteArray());
        if (!clean.isEmpty()) {
            a.data = clean;
            a.mimeType = QStringLiteral("image/jpeg");
            a.name = QFileInfo(a.name).completeBaseName() + QStringLiteral(".jpg");
            a.photoCleaned = true;
        }
    }
    a.photoUncleaned = a.mimeType.startsWith(QLatin1String("image/")) && !a.photoCleaned && a.mimeType != QLatin1String("image/gif")
        && a.mimeType != QLatin1String("image/svg+xml");
    m_attachments.append(a);
    Q_EMIT attachmentsChanged();
    touch();
}

void Composer::removeAttachment(int index)
{
    if (index >= 0 && index < m_attachments.size()) {
        m_attachments.removeAt(index);
        Q_EMIT attachmentsChanged();
        touch();
    }
}

void Composer::touch()
{
    if (m_started && !m_sending) {
        m_draftTimer.start();
    }
}

QByteArray Composer::build(bool draft, int *cleanedPhotos) const
{
    const Account *account = MailEngine::instance()->accountData(m_accountId);
    CamelMimeMessage *message = camel_mime_message_new();
    CamelInternetAddress *from = camel_internet_address_new();
    if (account) {
        camel_internet_address_add(from, account->fullName.toUtf8().constData(), account->email.toUtf8().constData());
    }
    camel_mime_message_set_from(message, from);
    g_object_unref(from);
    for (const auto &[type, text] : {std::pair{CAMEL_RECIPIENT_TYPE_TO, m_to}, std::pair{CAMEL_RECIPIENT_TYPE_CC, m_cc}, std::pair{CAMEL_RECIPIENT_TYPE_BCC, m_bcc}}) {
        CamelInternetAddress *list = addresses(text);
        camel_mime_message_set_recipients(message, type, list);
        g_object_unref(list);
    }
    camel_mime_message_set_subject(message, m_subject.toUtf8().constData());
    camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);
    // A random id at the sender's own domain: nothing about this PC or the
    // program in it.
    const QString domain = account ? account->email.section(QLatin1Char('@'), 1) : QStringLiteral("localhost");
    camel_mime_message_set_message_id(message, (QUuid::createUuid().toString(QUuid::Id128) + QLatin1Char('@') + domain).toUtf8().constData());
    if (!m_inReplyTo.isEmpty()) {
        camel_medium_set_header(CAMEL_MEDIUM(message), "In-Reply-To", ('<' + m_inReplyTo.toUtf8() + '>').constData());
        camel_medium_set_header(CAMEL_MEDIUM(message), "References", m_references.toUtf8().constData());
    }
    // No X-Mailer: what program wrote the mail is nobody's business.
    if (draft) {
        camel_medium_set_header(CAMEL_MEDIUM(message), "X-Wildos-Draft-Account", m_accountId.toUtf8().constData());
    }

    // Plain and HTML versions of the same words, so every mail program
    // shows them.
    CamelMultipart *alternative = camel_multipart_new();
    camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER(alternative), "multipart/alternative");
    camel_multipart_set_boundary(alternative, nullptr);
    CamelMimePart *plain = textPart(m_bodyText + quotePlain(m_quoteHeader, m_quote), "plain");
    QString html = m_bodyHtml;
    const QString quote = quoteHtml(m_quoteHeader, m_quote);
    if (html.contains(QLatin1String("</body>"))) {
        html.replace(QLatin1String("</body>"), quote + QStringLiteral("</body>"));
    } else {
        html += quote;
    }
    CamelMimePart *rich = textPart(html, "html");
    camel_multipart_add_part(alternative, plain);
    camel_multipart_add_part(alternative, rich);
    g_object_unref(plain);
    g_object_unref(rich);
    if (!m_calendarReply.isEmpty()) {
        CamelMimePart *calendar = camel_mime_part_new();
        const QByteArray ics = m_calendarReply.toUtf8();
        camel_mime_part_set_content(calendar, ics.constData(), ics.size(), "text/calendar; method=REPLY; charset=utf-8");
        camel_mime_part_set_encoding(calendar, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
        camel_multipart_add_part(alternative, calendar);
        g_object_unref(calendar);
    }

    int cleaned = 0;
    if (m_attachments.isEmpty()) {
        camel_medium_set_content(CAMEL_MEDIUM(message), CAMEL_DATA_WRAPPER(alternative));
    } else {
        CamelMultipart *mixed = camel_multipart_new();
        camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER(mixed), "multipart/mixed");
        camel_multipart_set_boundary(mixed, nullptr);
        CamelMimePart *body = camel_mime_part_new();
        camel_medium_set_content(CAMEL_MEDIUM(body), CAMEL_DATA_WRAPPER(alternative));
        camel_multipart_add_part(mixed, body);
        g_object_unref(body);
        for (const Attachment &a : m_attachments) {
            CamelMimePart *part = camel_mime_part_new();
            camel_mime_part_set_content(part, a.data.constData(), a.data.size(), a.mimeType.toUtf8().constData());
            camel_mime_part_set_filename(part, a.name.toUtf8().constData());
            camel_mime_part_set_disposition(part, "attachment");
            camel_mime_part_set_encoding(part, CAMEL_TRANSFER_ENCODING_BASE64);
            camel_multipart_add_part(mixed, part);
            g_object_unref(part);
            cleaned += a.photoCleaned ? 1 : 0;
        }
        camel_medium_set_content(CAMEL_MEDIUM(message), CAMEL_DATA_WRAPPER(mixed));
        g_object_unref(mixed);
    }
    g_object_unref(alternative);
    if (cleanedPhotos) {
        *cleanedPhotos = cleaned;
    }

    GByteArray *bytes = g_byte_array_new();
    CamelStream *stream = camel_stream_mem_new_with_byte_array(bytes);
    camel_data_wrapper_write_to_stream_sync(CAMEL_DATA_WRAPPER(message), stream, nullptr, nullptr);
    QByteArray out(reinterpret_cast<const char *>(bytes->data), int(bytes->len));
    g_object_unref(stream);
    g_object_unref(message);
    return out;
}

bool Composer::send()
{
    if (m_to.trimmed().isEmpty() && m_cc.trimmed().isEmpty() && m_bcc.trimmed().isEmpty()) {
        Q_EMIT problem(tr("Add someone to send this to."));
        return false;
    }
    for (const QString &field : {m_to, m_cc, m_bcc}) {
        QString bad;
        if (!validAddresses(field, &bad)) {
            Q_EMIT problem(tr("\"%1\" doesn't look like an email address.").arg(bad));
            return false;
        }
    }
    if (!MailEngine::instance()->accountData(m_accountId)) {
        Q_EMIT problem(tr("Add an account to send mail from."));
        return false;
    }
    m_draftTimer.stop();
    m_sending = true;
    Q_EMIT sendingChanged();
    int cleaned = 0;
    const QByteArray message = build(false, &cleaned);
    MailEngine *engine = MailEngine::instance();
    engine->countCleanedPhotos(cleaned);
    engine->queueOutbox(m_accountId, message);
    engine->sendOutbox();
    deleteDraft(m_draftUid);
    m_draftUid.clear();
    m_started = false;
    m_sending = false;
    Q_EMIT sendingChanged();
    Q_EMIT sent();
    return true;
}

void Composer::discard()
{
    m_draftTimer.stop();
    deleteDraft(m_draftUid);
    m_draftUid.clear();
    m_started = false;
}

void Composer::saveDraft()
{
    if (!m_started || m_accountId.isEmpty()) {
        return;
    }
    const QByteArray bytes = build(true, nullptr);
    const QString account = m_accountId;
    const QString previous = m_draftUid;
    auto uid = std::make_shared<QString>();
    auto ok = std::make_shared<bool>(false);
    MailEngine *engine = MailEngine::instance();
    engine->run([engine, account, bytes, previous, uid, ok]() {
        CamelFolder *drafts = engine->roleFolderSync(account, QStringLiteral("drafts"), nullptr);
        if (!drafts) {
            return;
        }
        CamelMimeMessage *message = camel_mime_message_new();
        CamelStream *stream = camel_stream_mem_new_with_buffer(bytes.constData(), bytes.size());
        camel_data_wrapper_construct_from_stream_sync(CAMEL_DATA_WRAPPER(message), stream, nullptr, nullptr);
        g_object_unref(stream);
        CamelMessageInfo *info = camel_message_info_new(nullptr);
        camel_message_info_set_flags(info, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN);
        gchar *appended = nullptr;
        *ok = camel_folder_append_message_sync(drafts, message, info, &appended, nullptr, nullptr);
        *uid = str(appended);
        g_free(appended);
        g_object_unref(info);
        g_object_unref(message);
        if (*ok && !previous.isEmpty()) {
            camel_folder_set_message_flags(drafts, previous.toUtf8().constData(), CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
        }
        camel_folder_synchronize_sync(drafts, TRUE, nullptr, nullptr);
    }, [this, uid, ok]() {
        if (!*ok) {
            return;
        }
        // Sent or thrown away while this draft was being filed.
        if (!m_started) {
            deleteDraft(*uid);
            return;
        }
        m_draftUid = *uid;
        m_savedAt = Formats::time(QDateTime::currentDateTime());
        Q_EMIT savedAtChanged();
    });
}

void Composer::deleteDraft(const QString &uid)
{
    if (uid.isEmpty()) {
        return;
    }
    const QString account = m_accountId;
    MailEngine *engine = MailEngine::instance();
    engine->run([engine, account, uid]() {
        if (CamelFolder *drafts = engine->roleFolderSync(account, QStringLiteral("drafts"), nullptr)) {
            camel_folder_set_message_flags(drafts, uid.toUtf8().constData(), CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
            camel_folder_synchronize_sync(drafts, TRUE, nullptr, nullptr);
        }
    });
}
