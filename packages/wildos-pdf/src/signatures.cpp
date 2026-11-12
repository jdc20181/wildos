#include "signatures.h"

#include <QBuffer>
#include <QFont>
#include <QFontMetricsF>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QUuid>

#include <qt6keychain/keychain.h>

#include <algorithm>
#include <cmath>

namespace
{
const QString service = QStringLiteral("Wild OS PDF");
const QString key = QStringLiteral("signatures");
// fonts-dancingscript, a Depends of this package.
const QString handwriting = QStringLiteral("Dancing Script");
const QColor ink(0x1f, 0x2a, 0x6b);
// Signatures are kept at this width, enough to stay sharp on a printout
// at the size a signature is usually placed.
constexpr int drawWidth = 1200;

// The picture cut down to what is drawn on it, with a little room round it.
QImage trimmed(const QImage &source, int margin)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    int left = image.width();
    int right = -1;
    int top = image.height();
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > 8) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < 0) {
        return {};
    }
    const QRect area = QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-margin, -margin, margin, margin).intersected(image.rect());
    return image.copy(area);
}

QJsonArray toJson(const QVariantList &strokes)
{
    QJsonArray out;
    for (const QVariant &stroke : strokes) {
        QJsonArray line;
        for (const QVariant &v : stroke.toList()) {
            const QPointF p = v.toPointF();
            // Four places are finer than any pen; the keyring entry stays small.
            line.append(std::round(p.x() * 10000) / 10000);
            line.append(std::round(p.y() * 10000) / 10000);
        }
        if (line.size() >= 4) {
            out.append(line);
        }
    }
    return out;
}

QImage renderDrawn(const QJsonArray &strokes, double aspect)
{
    aspect = std::clamp(aspect, 1.0, 8.0);
    const int height = int(drawWidth / aspect);
    QImage image(drawWidth, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    // The same weight as the pen in the drawing box, scaled up.
    QPen pen(ink, drawWidth * 0.0055, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    for (const QJsonValue &value : strokes) {
        const QJsonArray line = value.toArray();
        QPainterPath path;
        for (int i = 0; i + 1 < line.size(); i += 2) {
            const QPointF p(line.at(i).toDouble() * drawWidth, line.at(i + 1).toDouble() * height);
            if (i == 0) {
                path.moveTo(p);
            } else {
                path.lineTo(p);
            }
        }
        painter.drawPath(path);
    }
    painter.end();
    return trimmed(image, int(drawWidth * 0.01));
}

// A photo or scan of a signature on paper: the paper is made see-through
// so only the ink lands on the page. A picture that already has
// see-through parts is left as it is.
QImage fromPicture(const QImage &source)
{
    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    if (image.width() > drawWidth || image.height() > drawWidth / 2) {
        image = image.scaled(drawWidth, drawWidth / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    bool hasClear = false;
    for (int y = 0; y < image.height() && !hasClear; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) < 250) {
                hasClear = true;
                break;
            }
        }
    }
    if (!hasClear) {
        for (int y = 0; y < image.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QRgb c = line[x];
                const int darkest = std::min({qRed(c), qGreen(c), qBlue(c)});
                // Paper is light in every channel; ink is dark in at least
                // one. Between the two the edge fades, so lines stay smooth.
                const int alpha = darkest >= 225 ? 0 : darkest <= 175 ? 255 : (225 - darkest) * 255 / 50;
                line[x] = qRgba(qRed(c), qGreen(c), qBlue(c), alpha);
            }
        }
    }
    return trimmed(image, 6);
}

QByteArray pngBytes(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
}

Signatures *Signatures::instance()
{
    static Signatures *signatures = new Signatures;
    return signatures;
}

Signatures *Signatures::create(QQmlEngine *, QJSEngine *)
{
    Signatures *signatures = instance();
    QJSEngine::setObjectOwnership(signatures, QJSEngine::CppOwnership);
    return signatures;
}

Signatures::Signatures(QObject *parent)
    : QObject(parent)
{
}

QVariantList Signatures::items() const
{
    QMutexLocker locker(&m_lock);
    QVariantList out;
    for (const QJsonValue &value : m_entries) {
        const QJsonObject entry = value.toObject();
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (m_images.contains(id)) {
            out << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("kind"), entry.value(QStringLiteral("kind")).toString()}};
        }
    }
    return out;
}

QString Signatures::initials() const
{
    return m_initials;
}

int Signatures::revision() const
{
    return m_revision;
}

bool Signatures::ready() const
{
    return m_ready;
}

bool Signatures::kept() const
{
    return m_kept;
}

QImage Signatures::image(const QString &id) const
{
    QMutexLocker locker(&m_lock);
    return m_images.value(id);
}

QImage Signatures::initialsImage() const
{
    QMutexLocker locker(&m_lock);
    return m_initialsImage;
}

double Signatures::aspect(const QString &id) const
{
    const QImage picture = image(id);
    return picture.isNull() || picture.height() == 0 ? 3.0 : double(picture.width()) / picture.height();
}

double Signatures::initialsAspect() const
{
    const QImage picture = initialsImage();
    return picture.isNull() || picture.height() == 0 ? 1.5 : double(picture.width()) / picture.height();
}

QImage Signatures::renderTyped(const QString &text)
{
    const QString words = text.simplified();
    if (words.isEmpty()) {
        return {};
    }
    QFont font(handwriting);
    font.setPixelSize(150);
    const QFontMetricsF metrics(font);
    const QRectF bounds = metrics.boundingRect(words);
    const int width = int(std::ceil(bounds.width() + 120));
    const int height = int(std::ceil(metrics.height() + 120));
    QImage image(std::clamp(width, 1, 6000), height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(ink);
    painter.drawText(QPointF(60, 60 + metrics.ascent()), words);
    painter.end();
    QImage out = trimmed(image, 12);
    if (out.width() > drawWidth * 2) {
        out = out.scaledToWidth(drawWidth * 2, Qt::SmoothTransformation);
    }
    return out;
}

QImage Signatures::renderEntry(const QJsonObject &entry) const
{
    const QString kind = entry.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("draw")) {
        return renderDrawn(entry.value(QStringLiteral("strokes")).toArray(), entry.value(QStringLiteral("aspect")).toDouble(3.0));
    }
    if (kind == QLatin1String("type")) {
        return renderTyped(entry.value(QStringLiteral("text")).toString());
    }
    if (kind == QLatin1String("image")) {
        QImage picture;
        picture.loadFromData(QByteArray::fromBase64(entry.value(QStringLiteral("png")).toString().toLatin1()), "PNG");
        return picture.convertToFormat(QImage::Format_ARGB32);
    }
    return {};
}

void Signatures::rebuild()
{
    QHash<QString, QImage> images;
    for (const QJsonValue &value : std::as_const(m_entries)) {
        const QJsonObject entry = value.toObject();
        const QImage picture = renderEntry(entry);
        if (!picture.isNull()) {
            images.insert(entry.value(QStringLiteral("id")).toString(), picture);
        }
    }
    const QImage initialsPicture = renderTyped(m_initials);
    {
        QMutexLocker locker(&m_lock);
        m_images = images;
        m_initialsImage = initialsPicture;
        ++m_revision;
    }
    Q_EMIT changed();
}

void Signatures::load()
{
    auto *job = new QKeychain::ReadPasswordJob(service, this);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [this, job]() {
        if (job->error() == QKeychain::NoError) {
            const QJsonObject root = QJsonDocument::fromJson(job->textData().toUtf8()).object();
            {
                QMutexLocker locker(&m_lock);
                m_entries = root.value(QStringLiteral("signatures")).toArray();
            }
            m_initials = root.value(QStringLiteral("initials")).toString();
            m_kept = true;
        } else {
            // Nothing saved yet is not a fault; anything else means the
            // keyring is closed or missing.
            m_kept = job->error() == QKeychain::EntryNotFound;
            if (!m_kept) {
                qWarning("wildos-pdf: the keyring could not be read: %s", qPrintable(job->errorString()));
            }
        }
        rebuild();
        m_ready = true;
        Q_EMIT readyChanged();
    });
    job->start();
}

void Signatures::store()
{
    QJsonObject root;
    {
        QMutexLocker locker(&m_lock);
        root.insert(QStringLiteral("signatures"), m_entries);
    }
    root.insert(QStringLiteral("initials"), m_initials);
    auto *job = new QKeychain::WritePasswordJob(service, this);
    job->setKey(key);
    job->setTextData(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    connect(job, &QKeychain::Job::finished, this, [this, job]() {
        const bool ok = job->error() == QKeychain::NoError;
        if (!ok) {
            qWarning("wildos-pdf: the keyring did not take the signatures: %s", qPrintable(job->errorString()));
        }
        if (ok != m_kept) {
            m_kept = ok;
            Q_EMIT readyChanged();
        }
    });
    job->start();
}

QString Signatures::add(QJsonObject entry)
{
    const QImage picture = renderEntry(entry);
    if (picture.isNull()) {
        return {};
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.insert(QStringLiteral("id"), id);
    {
        QMutexLocker locker(&m_lock);
        m_entries.append(entry);
        m_images.insert(id, picture);
        ++m_revision;
    }
    Q_EMIT changed();
    store();
    return id;
}

QString Signatures::addDrawn(const QVariantList &strokes, double aspect)
{
    const QJsonArray lines = toJson(strokes);
    if (lines.isEmpty()) {
        return {};
    }
    return add(QJsonObject{{QStringLiteral("kind"), QStringLiteral("draw")}, {QStringLiteral("aspect"), aspect}, {QStringLiteral("strokes"), lines}});
}

QString Signatures::addTyped(const QString &text)
{
    if (text.simplified().isEmpty()) {
        return {};
    }
    return add(QJsonObject{{QStringLiteral("kind"), QStringLiteral("type")}, {QStringLiteral("text"), text.simplified()}});
}

QString Signatures::addImage(const QUrl &url)
{
    QImage picture(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (picture.isNull()) {
        return {};
    }
    const QImage ink = fromPicture(picture);
    if (ink.isNull()) {
        return {};
    }
    return add(QJsonObject{{QStringLiteral("kind"), QStringLiteral("image")}, {QStringLiteral("png"), QString::fromLatin1(pngBytes(ink).toBase64())}});
}

void Signatures::remove(const QString &id)
{
    {
        QMutexLocker locker(&m_lock);
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                m_entries.removeAt(i);
                break;
            }
        }
        m_images.remove(id);
        ++m_revision;
    }
    Q_EMIT changed();
    store();
}

void Signatures::setInitials(const QString &initials)
{
    const QString value = initials.simplified();
    if (value == m_initials) {
        return;
    }
    m_initials = value;
    const QImage picture = renderTyped(value);
    {
        QMutexLocker locker(&m_lock);
        m_initialsImage = picture;
        ++m_revision;
    }
    Q_EMIT changed();
    store();
}

SignatureProvider::SignatureProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage SignatureProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString name = id.section(QLatin1Char('/'), 0, 0);
    QImage picture = name == QLatin1String("initials") ? Signatures::instance()->initialsImage() : Signatures::instance()->image(name);
    if (!picture.isNull() && requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        picture = picture.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = picture.size();
    }
    return picture;
}
