#include "themeicon.h"

#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QQuickWindow>
#include <QUrl>

namespace
{
// Visible bounds as a share of the icon's square, per icon. Measuring means
// drawing the icon once and reading its alpha, so it is done once per icon
// for the whole process, not once per item.
QHash<QString, QRectF> &boundsCache()
{
    static QHash<QString, QRectF> cache;
    return cache;
}

// The icon drawn centred in a square, as the item shows it; an icon that is
// not square keeps its shape inside.
QImage renderSquare(const QIcon &icon, int side, qreal dpr)
{
    QImage canvas(QSize(side, side) * dpr, QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);
    const QPixmap pixmap = icon.pixmap(QSize(side, side), dpr);
    const QSizeF drawn = QSizeF(pixmap.size()) / pixmap.devicePixelRatio();
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRectF((side - drawn.width()) / 2, (side - drawn.height()) / 2, drawn.width(), drawn.height()), pixmap,
                       QRectF(pixmap.rect()));
    return canvas;
}
}

ThemeIcon::ThemeIcon(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setImplicitSize(24, 24);
    setAntialiasing(true);
}

void ThemeIcon::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        resolve();
    }
}

void ThemeIcon::setFallback(const QString &fallback)
{
    if (m_fallback != fallback) {
        m_fallback = fallback;
        resolve();
    }
}

void ThemeIcon::setSource(const QVariant &source)
{
    m_source = source;
    resolve();
}

void ThemeIcon::setTrim(bool trim)
{
    if (m_trim != trim) {
        m_trim = trim;
        Q_EMIT trimChanged();
        update();
    }
}

void ThemeIcon::setContentScale(qreal scale)
{
    scale = qBound(0.1, scale, 1.0);
    if (!qFuzzyCompare(m_contentScale, scale)) {
        m_contentScale = scale;
        Q_EMIT trimChanged();
        update();
    }
}

QIcon ThemeIcon::iconFor(const QString &name) const
{
    // A name may carry its own fallbacks, "x-office-spreadsheet,text-x-generic",
    // the form the Files menus and models already use.
    const QStringList names = name.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString source : names) {
        source = source.trimmed();
        if (source.startsWith(QLatin1String("file:"))) {
            source = QUrl(source).toLocalFile();
        }
        QIcon icon;
        if (source.startsWith(QLatin1Char('/'))) {
            if (QFileInfo::exists(source)) {
                icon = QIcon(source);
            }
        } else {
            icon = QIcon::fromTheme(source);
        }
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

void ThemeIcon::resolve()
{
    m_icon = QIcon();
    m_key.clear();
    switch (m_source.metaType().id()) {
    case QMetaType::QIcon:
        m_icon = m_source.value<QIcon>();
        m_key = m_icon.name().isEmpty() ? QStringLiteral("#%1").arg(m_icon.cacheKey()) : m_icon.name();
        break;
    case QMetaType::QImage:
        m_icon = QIcon(QPixmap::fromImage(m_source.value<QImage>()));
        m_key = QStringLiteral("#%1").arg(m_icon.cacheKey());
        break;
    case QMetaType::QPixmap:
        m_icon = QIcon(m_source.value<QPixmap>());
        m_key = QStringLiteral("#%1").arg(m_icon.cacheKey());
        break;
    case QMetaType::QUrl:
        m_icon = iconFor(m_source.toUrl().toString());
        m_key = m_source.toUrl().toString();
        break;
    case QMetaType::QString:
        m_icon = iconFor(m_source.toString());
        m_key = m_source.toString();
        break;
    default:
        break;
    }
    if (m_icon.isNull()) {
        m_icon = iconFor(m_name);
        m_key = m_name;
    }
    if (m_icon.isNull() && !m_fallback.isEmpty()) {
        m_icon = QIcon::fromTheme(m_fallback);
        m_key = m_fallback;
    }
    Q_EMIT nameChanged();
    update();
}

QRectF ThemeIcon::visibleBounds()
{
    const QRectF whole(0, 0, 1, 1);
    if (m_key.isEmpty()) {
        return whole;
    }
    auto &cache = boundsCache();
    const auto found = cache.constFind(m_key);
    if (found != cache.constEnd()) {
        return *found;
    }
    // Faint shadows and glows at the edge do not count as the icon.
    constexpr int side = 128;
    constexpr int threshold = 24;
    const QImage image = renderSquare(m_icon, side, 1.0).convertToFormat(QImage::Format_ARGB32);
    int left = side, top = side, right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > threshold) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    const QRectF bounds = right < left ? whole
                                       : QRectF(qreal(left) / side, qreal(top) / side, qreal(right - left + 1) / side, qreal(bottom - top + 1) / side);
    cache.insert(m_key, bounds);
    return bounds;
}

void ThemeIcon::paint(QPainter *painter)
{
    if (m_icon.isNull() || width() <= 0 || height() <= 0) {
        return;
    }
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    if (!m_trim) {
        const QSize logical{int(width()), int(height())};
        const QPixmap pixmap = m_icon.pixmap(logical, dpr);
        const QSizeF drawn = QSizeF(pixmap.size()) / pixmap.devicePixelRatio();
        const QRectF target((width() - drawn.width()) / 2, (height() - drawn.height()) / 2, drawn.width(), drawn.height());
        painter->drawPixmap(target, pixmap, QRectF(pixmap.rect()));
        return;
    }

    // The visible part fills the box's height (contentScale of the item) and
    // sits in its centre, so a row of icons shares one height whatever each
    // file's shape: fitting both ways left wide icons, like a folder, shorter
    // than tall ones. A very wide icon may run up to a quarter past the box
    // before it is held to that width. The whole icon is drawn as large as
    // that needs, so it is rendered sharp at that size rather than stretched.
    const QRectF bounds = visibleBounds();
    const qreal boxW = width() * m_contentScale;
    const qreal boxH = height() * m_contentScale;
    const qreal full = qMin(boxW * 1.25 / bounds.width(), boxH / bounds.height());
    const int side = qMax(1, qRound(full));
    const QImage image = renderSquare(m_icon, side, dpr);
    const qreal scale = full / side;
    const QPointF centre = bounds.center() * full;
    const QRectF target(width() / 2 - centre.x(), height() / 2 - centre.y(), side * scale, side * scale);
    painter->drawImage(target, image, QRectF(image.rect()));
}
