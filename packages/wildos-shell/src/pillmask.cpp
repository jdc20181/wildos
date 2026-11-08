#include "pillmask.h"

#include <QPainterPath>
#include <QPolygon>
#include <QRectF>

QRegion PillMask::region(const QVariantList &rects, qreal radius) const
{
    QRegion result;
    for (const QVariant &value : rects) {
        const QRectF rect = value.toRectF();
        if (rect.isEmpty()) {
            continue;
        }
        QPainterPath path;
        path.addRoundedRect(rect, radius, radius);
        result += QRegion(path.toFillPolygon().toPolygon());
    }
    return result;
}
