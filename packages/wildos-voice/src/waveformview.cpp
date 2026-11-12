#include "waveformview.h"

#include "audio.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int pitch = WaveformView::barWidth + WaveformView::gap;
}

WaveformView::WaveformView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

QVariantList WaveformView::bars() const
{
    QVariantList list;
    list.reserve(m_bars.size());
    for (float bar : m_bars) {
        list.append(bar);
    }
    return list;
}

void WaveformView::setBars(const QVariantList &bars)
{
    m_bars.clear();
    m_bars.reserve(bars.size());
    for (const QVariant &bar : bars) {
        m_bars.append(std::clamp(bar.toFloat(), 0.0f, 1.0f));
    }
    Q_EMIT barsChanged();
    update();
}

void WaveformView::setLive(bool live)
{
    if (live != m_live) {
        m_live = live;
        Q_EMIT changed();
        Q_EMIT geometryChanged();
        update();
    }
}

void WaveformView::setProgress(double progress)
{
    progress = std::clamp(progress, 0.0, 1.0);
    if (!qFuzzyCompare(progress + 1, m_progress + 1)) {
        m_progress = progress;
        Q_EMIT changed();
        Q_EMIT geometryChanged();
        update();
    }
}

void WaveformView::setBarColor(const QColor &color)
{
    m_barColor = color;
    Q_EMIT changed();
    update();
}

void WaveformView::setPlayedColor(const QColor &color)
{
    m_playedColor = color;
    Q_EMIT changed();
    update();
}

void WaveformView::setDotColor(const QColor &color)
{
    m_dotColor = color;
    Q_EMIT changed();
    update();
}

void WaveformView::setPlayheadColor(const QColor &color)
{
    m_playheadColor = color;
    Q_EMIT changed();
    update();
}

void WaveformView::setShowPlayhead(bool show)
{
    if (show != m_showPlayhead) {
        m_showPlayhead = show;
        Q_EMIT changed();
        update();
    }
}

void WaveformView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    Q_EMIT geometryChanged();
}

int WaveformView::slotCount() const
{
    return std::max(1, int((width() + gap) / pitch));
}

int WaveformView::shownSlots() const
{
    // A short recording keeps one bar per slot, centred, rather than
    // stretching a few bars into blocks.
    if (!m_live && !m_bars.isEmpty()) {
        return std::min(slotCount(), int(m_bars.size()));
    }
    return slotCount();
}

double WaveformView::left() const
{
    return std::floor((width() - (shownSlots() * pitch - gap)) / 2.0);
}

double WaveformView::slotX(int slot) const
{
    return left() + slot * pitch;
}

double WaveformView::playheadX() const
{
    if (m_live) {
        return slotX(int(std::lround(slotCount() * livePlayhead))) - (gap + 2) / 2.0;
    }
    return fullX(m_progress);
}

double WaveformView::fullX(double fraction) const
{
    return left() + std::clamp(fraction, 0.0, 1.0) * (shownSlots() * pitch - gap);
}

double WaveformView::fractionAt(double x) const
{
    return std::clamp((x - left()) / std::max(1, shownSlots() * pitch - gap), 0.0, 1.0);
}

double WaveformView::liveX(double agoMs) const
{
    const int head = int(std::lround(slotCount() * livePlayhead));
    const double slot = head - 0.5 - agoMs / Audio::barMs;
    if (slot < 0) {
        return -1;
    }
    return slotX(0) + slot * pitch + barWidth / 2.0;
}

void WaveformView::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    const int slots = slotCount();
    const double middle = height() / 2.0;
    const double tallest = height() * 0.8;
    auto drawBar = [&](int slot, float level, const QColor &color) {
        const double h = std::max(4.0, 4.0 + double(level) * (tallest - 4.0));
        painter->setBrush(color);
        painter->drawRoundedRect(QRectF(slotX(slot), middle - h / 2.0, barWidth, h), 2, 2);
    };
    auto drawDot = [&](int slot) {
        painter->setBrush(m_dotColor);
        painter->drawRoundedRect(QRectF(slotX(slot), middle - 2, barWidth, 4), 2, 2);
    };

    if (m_live) {
        const int head = int(std::lround(slots * livePlayhead));
        const qsizetype shown = std::min<qsizetype>(head, m_bars.size());
        const int first = head - int(shown);
        for (int slot = 0; slot < first; ++slot) {
            drawDot(slot);
        }
        for (int i = 0; i < shown; ++i) {
            const int slot = first + i;
            QColor color = m_barColor;
            // The board fades older bars from 0.55 to full at the playhead.
            color.setAlphaF(color.alphaF() * float(0.55 + 0.45 * double(slot) / std::max(1, head - 1)));
            drawBar(slot, m_bars.at(m_bars.size() - shown + i), color);
        }
        for (int slot = head; slot < slots; ++slot) {
            drawDot(slot);
        }
    } else if (m_bars.isEmpty()) {
        for (int slot = 0; slot < slots; ++slot) {
            drawDot(slot);
        }
    } else {
        const int count = shownSlots();
        const double played = m_progress * count;
        for (int slot = 0; slot < count; ++slot) {
            // Each slot shows the loudest bar it covers, so a long recording
            // keeps its peaks when it is squeezed to fit.
            const qsizetype from = qsizetype(double(slot) * m_bars.size() / count);
            const qsizetype to = std::max(from + 1, qsizetype(double(slot + 1) * m_bars.size() / count));
            float level = 0.0f;
            for (qsizetype i = from; i < to && i < m_bars.size(); ++i) {
                level = std::max(level, m_bars.at(i));
            }
            drawBar(slot, level, slot < played ? m_playedColor : m_barColor);
        }
    }

    if (m_showPlayhead) {
        painter->setBrush(m_playheadColor);
        painter->drawRect(QRectF(std::round(playheadX()), 0, 2, height()));
    }
}
