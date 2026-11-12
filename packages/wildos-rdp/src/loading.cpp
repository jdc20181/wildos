#include "loading.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

namespace
{
// The board's loop and where the window starts in it (wlwin begins at 2%).
constexpr double loop = 9.0;
constexpr double introOffset = 0.18;
// Where the board's app is ready: the light has run its laps and the full
// trace starts (wlcomplete, 59.5%).
constexpr double readyPercent = 59.5;
constexpr double lap = 1.75;
constexpr double boardLength = 441;
constexpr double slowAfter = 20;
constexpr double titleBar = 40;

// branding/logo/wild-os-mark.svg, as the board has it.
const QPointF wPoints[] = {{37, 17},        {50.34, 40},      {64.75, 64.84}, {79.16, 40},     {92.5, 17},  {105.84, 40},
                           {134.26, 89},   {120.76, 112.28}, {92.5, 63.55},  {64.91, 111.12}, {23.66, 40}};
const QPointF sparkPoints[] = {{148.5, 17}, {162, 40}, {148.5, 63}, {135, 40}};
const QPointF sparkCentre(148.5, 40);

// CSS cubic-bezier easing.
double bezier(double u, double x1, double y1, double x2, double y2)
{
    if (u <= 0) {
        return 0;
    }
    if (u >= 1) {
        return 1;
    }
    double lo = 0, hi = 1;
    for (int n = 0; n < 28; ++n) {
        const double m = (lo + hi) / 2;
        const double x = 3 * (1 - m) * (1 - m) * m * x1 + 3 * (1 - m) * m * m * x2 + m * m * m;
        (x < u ? lo : hi) = m;
    }
    const double m = (lo + hi) / 2;
    return 3 * (1 - m) * (1 - m) * m * y1 + 3 * (1 - m) * m * m * y2 + m * m * m;
}

double ease(double u)
{
    return bezier(u, 0.4, 0, 0.2, 1);
}

// A keyframed value at x percent of the loop: the board's @keyframes, each
// step eased as CSS eases it.
double keyframes(double x, std::initializer_list<std::pair<double, double>> stops, double (*curve)(double) = ease)
{
    const auto *first = stops.begin();
    const auto *last = stops.end() - 1;
    if (x <= first->first) {
        return first->second;
    }
    if (x >= last->first) {
        return last->second;
    }
    for (const auto *s = first; s != last; ++s) {
        const auto *next = s + 1;
        if (x <= next->first) {
            const double span = next->first - s->first;
            const double u = span > 0 ? curve((x - s->first) / span) : 1;
            return s->second + (next->second - s->second) * u;
        }
    }
    return last->second;
}

double completeCurve(double u)
{
    return bezier(u, 0.45, 0, 0.2, 1);
}

QLinearGradient wla()
{
    QLinearGradient g(0, 17, 0, 113);
    g.setColorAt(0, QColor(0xb4, 0xca, 0xff));
    g.setColorAt(0.5, QColor(0x7d, 0x97, 0xec));
    g.setColorAt(1, QColor(0xf0, 0xa3, 0xae));
    return g;
}

QLinearGradient wlt()
{
    QLinearGradient g(20, 0, 165, 0);
    g.setColorAt(0, QColor(0xcf, 0xdc, 0xff));
    g.setColorAt(0.5, QColor(0xff, 0xff, 0xff));
    g.setColorAt(1, QColor(0xff, 0xd2, 0xc8));
    return g;
}

QFont font(double px, int weight = QFont::Normal)
{
    QFont f(QStringLiteral("Albert Sans"));
    f.setPixelSize(int(std::lround(px)));
    f.setWeight(QFont::Weight(weight));
    return f;
}

QColor rgba(int r, int g, int b, double a)
{
    return QColor::fromRgbF(r / 255.0, g / 255.0, b / 255.0, a);
}

// Part of the W's outline: `length` units from `from`, going round, in the
// board's units (the outline is 441 long there).
QPainterPath along(double from, double length, double total)
{
    const double scale = total / boardLength;
    from = std::fmod(from * scale, total);
    if (from < 0) {
        from += total;
    }
    length = std::min(length * scale, total);
    QPainterPath out;
    const int n = int(std::size(wPoints));
    double at = 0;
    bool started = false;
    double remaining = length;
    // Twice round, so a piece that wraps past the start is drawn whole.
    for (int i = 0; i < 2 * n && remaining > 0; ++i) {
        const QPointF a = wPoints[i % n];
        const QPointF b = wPoints[(i + 1) % n];
        const double edge = std::hypot(b.x() - a.x(), b.y() - a.y());
        const double end = at + edge;
        if (end > from) {
            const double s0 = std::max(from, at);
            const double take = std::min(end - s0, remaining);
            const QPointF p0 = a + (b - a) * ((s0 - at) / edge);
            const QPointF p1 = a + (b - a) * ((s0 + take - at) / edge);
            if (!started) {
                out.moveTo(p0);
                started = true;
            }
            out.lineTo(p1);
            remaining -= take;
            from = s0 + take;
        }
        at = end;
    }
    return out;
}

void glowStroke(QPainter &p, const QPainterPath &path, double opacity, bool roundCap)
{
    const double base = p.opacity();
    // drop-shadow(0 0 3px rgba(185,207,255,0.95)) drop-shadow(0 0 10px
    // rgba(143,180,255,0.6)), drawn as soft wide strokes under the line.
    const Qt::PenCapStyle cap = roundCap ? Qt::RoundCap : Qt::FlatCap;
    const struct {
        double width;
        QColor colour;
    } layers[] = {
        {16, rgba(143, 180, 255, 0.05)}, {11, rgba(143, 180, 255, 0.08)}, {7, rgba(143, 180, 255, 0.12)},
        {4.5, rgba(185, 207, 255, 0.30)}, {2.8, rgba(185, 207, 255, 0.55)},
    };
    for (const auto &l : layers) {
        QColor c = l.colour;
        c.setAlphaF(c.alphaF() * opacity);
        p.strokePath(path, QPen(c, l.width, Qt::SolidLine, cap, Qt::RoundJoin));
    }
    QPen line(QBrush(wlt()), 1.4, Qt::SolidLine, cap, Qt::RoundJoin);
    p.setOpacity(base * opacity);
    p.strokePath(path, line);
    p.setOpacity(base);
}

// Text laid out in lines of a set height, centred, as the board's
// paragraphs are.
double drawParagraph(QPainter &p, const QString &text, const QFont &f, double width, double lineHeight, const QPointF &topCentre, bool draw)
{
    QTextLayout layout(text, f);
    layout.beginLayout();
    double y = 0;
    std::vector<std::pair<QTextLine, double>> lines;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        lines.emplace_back(line, y);
        y += lineHeight;
    }
    layout.endLayout();
    if (draw) {
        for (auto &[line, top] : lines) {
            const double x = topCentre.x() - line.naturalTextWidth() / 2 - line.x();
            line.draw(&p, QPointF(x, topCentre.y() + top + (lineHeight - line.height()) / 2));
        }
    }
    return y;
}
}

LoadingView::LoadingView()
{
    m_w.moveTo(wPoints[0]);
    m_length = 0;
    for (size_t i = 1; i <= std::size(wPoints); ++i) {
        const QPointF a = wPoints[i - 1];
        const QPointF b = wPoints[i % std::size(wPoints)];
        m_length += std::hypot(b.x() - a.x(), b.y() - a.y());
        if (i < std::size(wPoints)) {
            m_w.lineTo(b);
        }
    }
    m_w.closeSubpath();
    m_spark.moveTo(sparkPoints[0]);
    for (size_t i = 1; i < std::size(sparkPoints); ++i) {
        m_spark.lineTo(sparkPoints[i]);
    }
    m_spark.closeSubpath();
}

void LoadingView::start(const QString &appName, const QIcon &icon, bool reducedMotion)
{
    m_app = appName;
    m_icon = icon;
    m_still = reducedMotion;
    m_stage = Stage::Starting;
    m_finishAt = -1;
    m_clock.start();
}

double LoadingView::seconds() const
{
    return m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0;
}

void LoadingView::setStage(Stage stage)
{
    if (stage != m_stage) {
        m_stage = stage;
        m_stageAt = seconds();
    }
}

void LoadingView::setReason(const QString &reason, int percent, const QString &progress)
{
    m_reason = reason;
    m_percent = percent;
    m_progress = progress;
}

void LoadingView::finish()
{
    if (m_finishAt < 0) {
        m_finishAt = seconds();
    }
}

bool LoadingView::slow() const
{
    return m_ended || (m_finishAt < 0 && !m_arrived && seconds() >= slowAfter);
}

void LoadingView::setEnded()
{
    m_ended = true;
    m_reason = QStringLiteral("It may have run into a problem. You can try opening it again.");
    m_percent = -1;
}

double LoadingView::introPercent() const
{
    if (m_still) {
        return readyPercent;
    }
    return std::min((seconds() + introOffset) / loop * 100, readyPercent);
}

double LoadingView::finishPercent() const
{
    if (m_finishAt < 0) {
        return introPercent();
    }
    if (m_still) {
        return 100;
    }
    return std::min(readyPercent + (seconds() - m_finishAt) / loop * 100, 100.0);
}

bool LoadingView::done() const
{
    return m_finishAt >= 0 && finishPercent() >= 79;
}

bool LoadingView::animating() const
{
    // The light keeps running its laps however long opening takes: a still
    // picture on screen reads as something that has stopped. Once the app
    // has ended nothing is under way, and nothing moves.
    return !m_still && !m_ended;
}

double LoadingView::appOpacity() const
{
    if (m_finishAt < 0) {
        return 0;
    }
    return keyframes(finishPercent(), {{70, 0}, {79, 1}});
}

QRectF LoadingView::buttonRect(Hit which, const QSizeF &size) const
{
    switch (which) {
    case Hit::Minimise: return QRectF(size.width() - 138, 0, 46, titleBar);
    case Hit::Maximise: return QRectF(size.width() - 92, 0, 46, titleBar);
    case Hit::Close: return QRectF(size.width() - 46, 0, 46, titleBar);
    default: return {};
    }
}

void LoadingView::paintTitleBar(QPainter &p, const QSizeF &size, double chipOpacity) const
{
    p.fillRect(QRectF(0, 0, size.width(), titleBar), rgba(9, 11, 15, 0.55));
    double x = 14;
    if (!m_icon.isNull()) {
        m_icon.paint(&p, QRect(int(x), 11, 18, 18));
    }
    x += 18 + 10;
    const QFont name = font(13, 480);
    p.setFont(name);
    p.setPen(QColor(0xee, 0xf1, 0xf8));
    const double nameWidth = QFontMetricsF(name).horizontalAdvance(m_app);
    p.drawText(QRectF(x, 0, nameWidth + 2, titleBar), Qt::AlignVCenter | Qt::AlignLeft, m_app);
    x += nameWidth + 10;
    if (chipOpacity > 0) {
        const QFont chipFont = font(11);
        const QString chip = QStringLiteral("Windows");
        const double w = QFontMetricsF(chipFont).horizontalAdvance(chip) + 16;
        const QRectF r(x, 10, w, 20);
        const double before = p.opacity();
        p.setOpacity(before * chipOpacity);
        p.setPen(Qt::NoPen);
        p.setBrush(rgba(143, 180, 255, 0.14));
        p.drawRoundedRect(r, 10, 10);
        p.setFont(chipFont);
        p.setPen(QColor(0xb9, 0xcf, 0xff));
        p.drawText(r, Qt::AlignCenter, chip);
        p.setOpacity(before);
    }
    const QColor ink(0xee, 0xf1, 0xf8);
    for (Hit b : {Hit::Minimise, Hit::Maximise, Hit::Close}) {
        const QRectF r = buttonRect(b, size);
        if (m_hover == b) {
            p.fillRect(r, rgba(255, 255, 255, 0.08));
        }
        p.save();
        // The glyphs are 11 px drawings of a 12 unit box.
        p.translate(r.center().x() - 5.5, r.center().y() - 5.5);
        p.scale(11.0 / 12, 11.0 / 12);
        p.setBrush(Qt::NoBrush);
        if (b == Hit::Minimise) {
            p.setPen(QPen(ink, 1.3));
            p.drawLine(QPointF(1.5, 6), QPointF(10.5, 6));
        } else if (b == Hit::Maximise) {
            p.setPen(QPen(ink, 1.2));
            p.drawRoundedRect(QRectF(1.5, 1.5, 9, 9), 1.8, 1.8);
        } else {
            p.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(2, 2), QPointF(10, 10));
            p.drawLine(QPointF(10, 2), QPointF(2, 10));
        }
        p.restore();
    }
}

void LoadingView::paintMark(QPainter &p, const QPointF &origin, double x, double t) const
{
    p.save();
    // The board's 150 px square with viewBox "18 -10.5 150 150".
    p.translate(origin.x() - 18, origin.y() + 10.5);
    const double o = p.opacity();

    // wlbase: the dim W the light runs over.
    const double base = m_still ? 1 : keyframes(x, {{9, 0}, {13, 1}, {62, 1}, {70, 0}});
    if (base > 0) {
        p.setOpacity(o * (base * 0.12));
        p.fillPath(m_w, wla());
        p.setOpacity(o * (base));
        p.strokePath(m_w, QPen(rgba(201, 216, 255, 0.24), 0.9, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.setOpacity(o);
    }

    // wltraceon and wltr: a 64 unit light running laps, 1.75 s each. Board
    // 9.13 stops it when opening takes longer; it keeps running here, so
    // the slow state never looks like a crash.
    if (!m_still) {
        const double on = m_ended ? 0 : keyframes(x, {{12, 0}, {14, 1}, {59.5, 1}, {61.5, 0}});
        if (on > 0) {
            const double offset = boardLength * (1 - std::fmod(t + introOffset, lap) / lap);
            glowStroke(p, along(boardLength - offset, 64, m_length), on, true);
        }
    }

    // wlfill: the W in full colour as the app arrives.
    const double fill = keyframes(x, {{62, 0}, {66.5, 1}, {71, 1}, {76, 0}});
    if (fill > 0) {
        p.setOpacity(o * (fill));
        p.fillPath(m_w, wla());
        p.setOpacity(o);
    }

    // wldraw: the outline drawing itself in.
    if (!m_still) {
        const double drawn = boardLength - keyframes(x, {{0, 441}, {4, 441}, {11, 0}});
        const double opacity = keyframes(x, {{0, 1}, {11, 1}, {14, 0}});
        if (drawn > 0 && opacity > 0) {
            p.setOpacity(o * (opacity));
            p.strokePath(along(0, drawn, m_length), QPen(QColor(0xc9, 0xd8, 0xff), 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
            p.setOpacity(o);
        }
    }

    // wlcomplete: one full trace once the app is ready.
    if (m_finishAt >= 0 && !m_still) {
        const double drawn = boardLength - keyframes(x, {{59.5, 441}, {66, 0}}, completeCurve);
        const double opacity = keyframes(x, {{66, 1}, {70, 0}});
        if (drawn > 0 && opacity > 0) {
            glowStroke(p, along(0, drawn, m_length), opacity, false);
        }
    }

    // wlflash: the spark's flash.
    const double flash = keyframes(x, {{64.9, 0}, {67, 0.95}, {73, 0}});
    if (flash > 0) {
        const double s = keyframes(x, {{64.9, 0.3}, {67, 1}, {73, 1.9}});
        QRadialGradient g(sparkCentre, 34 * s);
        g.setColorAt(0, rgba(255, 255, 255, 0.95));
        g.setColorAt(0.45, rgba(185, 207, 255, 0.4));
        g.setColorAt(1, rgba(185, 207, 255, 0));
        p.setOpacity(o * (std::min(1.0, flash / 0.95)));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(sparkCentre, 34 * s, 34 * s);
        p.setOpacity(o);
    }

    // wldia: the spark pops in, pulses when the app is ready, and goes.
    const double dia = m_still ? 1 : keyframes(x, {{3, 0}, {6.5, 1}, {69, 1}, {74, 0}});
    if (dia > 0) {
        const double s = m_still ? 1 : keyframes(x, {{3, 0}, {6.5, 1.3}, {8.5, 1}, {63, 1}, {66.5, 1.35}, {69, 1}, {74, 0.6}});
        const double turn = m_still ? 0 : keyframes(x, {{3, -45}, {6.5, 0}});
        const double shift = m_still ? 0 : keyframes(x, {{69, 0}, {74, 1}});
        p.save();
        p.translate(sparkCentre + QPointF(10 * shift, -10 * shift));
        p.rotate(turn);
        p.scale(s, s);
        p.translate(-sparkCentre);
        p.setOpacity(o * (dia));
        p.fillPath(m_spark, wla());
        p.restore();
    }
    p.restore();
}

void LoadingView::paint(QPainter &p, const QSizeF &size, bool background, bool foreground) const
{
    const double t = seconds();
    const double x = finishPercent();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // wlwin: the window fades and grows in from 97%, about 50% 40%.
    const double intro = m_still ? 1 : keyframes(std::min((t + introOffset) / loop * 100, 4.5), {{2, 0}, {4.5, 1}});
    const double introScale = m_still ? 1 : keyframes(std::min((t + introOffset) / loop * 100, 4.5), {{2, 0.97}, {4.5, 1}});
    // wlloader: the loader fades out (and grows to 104%) as the app fades in.
    const double loader = keyframes(x, {{70, 1}, {78, 0}});
    const double loaderScale = keyframes(x, {{70, 1}, {78, 1.04}});
    const double chip = keyframes(x, {{70, 1}, {78, 0}});

    p.translate(size.width() / 2, size.height() * 0.4);
    p.scale(introScale, introScale);
    p.translate(-size.width() / 2, -size.height() * 0.4);
    p.setOpacity(intro * loader);

    QPainterPath frame;
    frame.addRoundedRect(QRectF(QPointF(0, 0), size).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    if (background) {
        p.fillPath(frame, rgba(23, 25, 29, 0.94));
    }
    p.save();
    p.setClipPath(frame);

    // The body's glow: radial-gradient(ellipse at 50% 42%, rgba(61,91,201,
    // 0.20), transparent 60%), sized to the farthest corner.
    const QRectF body(0, titleBar, size.width(), size.height() - titleBar);
    if (background) {
        const QPointF c(body.center().x(), body.top() + body.height() * 0.42);
        const double rx = body.width() / 2 * M_SQRT2;
        const double ry = std::max(c.y() - body.top(), body.bottom() - c.y()) * M_SQRT2;
        p.save();
        p.translate(c);
        p.scale(1, ry / rx);
        QRadialGradient g(QPointF(0, 0), rx);
        g.setColorAt(0, rgba(61, 91, 201, 0.20));
        g.setColorAt(0.6, rgba(23, 25, 29, 0));
        p.fillRect(QRectF(-rx, -rx, 2 * rx, 2 * rx), g);
        p.restore();
    }

    if (background) {
        paintTitleBar(p, size, chip);
    }
    if (!foreground) {
        p.restore();
        p.setOpacity(intro * loader);
        p.setPen(QPen(rgba(255, 255, 255, 0.11), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(frame);
        p.restore();
        return;
    }

    // The body, scaled with wlloader about its centre.
    p.save();
    p.translate(body.center());
    p.scale(loaderScale, loaderScale);
    p.translate(-body.center());

    const bool isSlow = slow();
    const QFont titleFont = font(22, 480);
    const QFont bodyFont = font(14);
    const double titleHeight = 27;
    double contentHeight = 150 - 40 + 26 + titleHeight + 10;
    double reasonHeight = 0;
    const bool bar = isSlow && m_percent >= 0;
    if (isSlow) {
        reasonHeight = drawParagraph(p, m_reason, bodyFont, 440, 14 * 1.55, QPointF(), false);
        contentHeight += reasonHeight + 26 + 34;
        if (bar) {
            contentHeight += 4 + 8 + 15 + 26;
        }
    } else {
        contentHeight += 20;
    }
    const double markTop = body.top() + (body.height() - contentHeight) / 2 - 40;
    paintMark(p, QPointF(body.center().x() - 75, markTop), x, t);

    double y = markTop + 150 + 26;
    const double title = m_still ? 1 : keyframes(x, {{6, 0}, {10, 1}, {70, 1}, {74, 0}});
    const double rise = m_still ? 0 : keyframes(x, {{6, 6}, {10, 0}});
    p.setOpacity(intro * loader * title);
    p.setFont(titleFont);
    p.setPen(Qt::white);
    const QString heading = (m_ended  ? QStringLiteral("%1 closed before it opened")
                             : isSlow ? QStringLiteral("Still opening %1")
                                      : QStringLiteral("Opening %1"))
                                .arg(m_app);
    p.drawText(QRectF(0, y + rise, size.width(), titleHeight), Qt::AlignCenter, heading);
    y += titleHeight + 10;

    p.setFont(bodyFont);
    if (!isSlow) {
        // wls1 and wls2: "Starting Windows mode", then "Almost ready" once
        // the app's own process is up and drawing.
        const double in = m_still ? 1 : keyframes(x, {{10, 0}, {13, 1}});
        const double out = keyframes(x, {{68, 1}, {71, 0}});
        const double fade = 0.27;
        const double since = t - m_stageAt;
        const double almost = m_stage == Stage::AlmostReady ? (m_still ? 1 : std::clamp(since / fade, 0.0, 1.0)) : 0;
        const QRectF line(0, y, size.width(), 20);
        p.setPen(rgba(232, 236, 248, 0.85));
        p.setOpacity(intro * loader * 0.75 * in * out * (1 - almost));
        p.drawText(line, Qt::AlignCenter, QStringLiteral("Starting Windows mode"));
        p.setOpacity(intro * loader * 0.75 * out * almost);
        p.drawText(line, Qt::AlignCenter, QStringLiteral("Almost ready"));
    } else {
        p.setOpacity(intro * loader);
        p.setPen(rgba(232, 236, 248, 0.85));
        drawParagraph(p, m_reason, bodyFont, 440, 14 * 1.55, QPointF(size.width() / 2, y), true);
        y += reasonHeight + 26;
        if (bar) {
            const QRectF track(size.width() / 2 - 160, y, 320, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(rgba(255, 255, 255, 0.1));
            p.drawRoundedRect(track, 2, 2);
            p.setBrush(QColor(0x8f, 0xb4, 0xff));
            p.drawRoundedRect(QRectF(track.topLeft(), QSizeF(320 * std::clamp(m_percent, 0, 100) / 100.0, 4)), 2, 2);
            p.setFont(font(12));
            p.setPen(rgba(232, 236, 248, 0.6));
            p.drawText(QRectF(0, y + 4 + 8, size.width(), 15), Qt::AlignCenter, m_progress.isEmpty() ? QStringLiteral("%1% done").arg(m_percent) : m_progress);
            y += 4 + 8 + 15 + 26;
        }
        const QFont buttonFont = font(13);
        QFontMetricsF fm(buttonFont);
        const auto [first, second] = buttons();
        const double cancelW = fm.horizontalAdvance(first.first) + 32;
        const double detailsW = fm.horizontalAdvance(second.first) + 32;
        double bx = size.width() / 2 - (cancelW + 8 + detailsW) / 2;
        p.setFont(buttonFont);
        for (const auto &[label, w, hit] : {std::tuple{first.first, cancelW, first.second},
                                            std::tuple{second.first, detailsW, second.second}}) {
            const QRectF r(bx, y, w, 34);
            p.setPen(QPen(rgba(255, 255, 255, 0.12), 1));
            p.setBrush(m_hover == hit ? rgba(255, 255, 255, 0.10) : rgba(255, 255, 255, 0.05));
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
            p.setPen(QColor(0xee, 0xf1, 0xf8));
            p.drawText(r, Qt::AlignCenter, label);
            bx += w + 8;
        }
    }
    p.restore();
    p.restore();
    if (background) {
        p.setOpacity(intro * loader);
        p.setPen(QPen(rgba(255, 255, 255, 0.11), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(frame);
    }
    p.restore();
}

LoadingView::Hit LoadingView::hitTest(const QPointF &at, const QSizeF &size) const
{
    for (Hit b : {Hit::Minimise, Hit::Maximise, Hit::Close}) {
        if (buttonRect(b, size).contains(at)) {
            return b;
        }
    }
    if (at.y() < titleBar) {
        return Hit::Drag;
    }
    if (!slow()) {
        return Hit::None;
    }
    // The two buttons sit at the bottom of the centred column; recompute
    // their place the way paint() lays it out.
    const QSizeF bodySize(size.width(), size.height() - titleBar);
    QFont bodyFont = font(14);
    QImage scratch(1, 1, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&scratch);
    const double reasonHeight = drawParagraph(p, m_reason, bodyFont, 440, 14 * 1.55, QPointF(), false);
    const bool bar = m_percent >= 0;
    double contentHeight = 150 - 40 + 26 + 27 + 10 + reasonHeight + 26 + 34 + (bar ? 4 + 8 + 15 + 26 : 0);
    const double markTop = titleBar + (bodySize.height() - contentHeight) / 2 - 40;
    const double y = markTop + 150 + 26 + 27 + 10 + reasonHeight + 26 + (bar ? 4 + 8 + 15 + 26 : 0);
    QFontMetricsF fm(font(13));
    const auto [first, second] = buttons();
    const double cancelW = fm.horizontalAdvance(first.first) + 32;
    const double detailsW = fm.horizontalAdvance(second.first) + 32;
    const double bx = size.width() / 2 - (cancelW + 8 + detailsW) / 2;
    if (QRectF(bx, y, cancelW, 34).contains(at)) {
        return first.second;
    }
    if (QRectF(bx + cancelW + 8, y, detailsW, 34).contains(at)) {
        return second.second;
    }
    return Hit::None;
}

std::pair<std::pair<QString, LoadingView::Hit>, std::pair<QString, LoadingView::Hit>> LoadingView::buttons() const
{
    if (m_ended) {
        return {{QStringLiteral("Try again"), Hit::Retry}, {QStringLiteral("Close"), Hit::Cancel}};
    }
    return {{QStringLiteral("Cancel"), Hit::Cancel}, {QStringLiteral("Show details"), Hit::Details}};
}

QRectF LoadingView::steadyArea(const QSizeF &size) const
{
    const bool isSlow = slow();
    double contentHeight = 150 - 40 + 26 + 27 + 10 + 20;
    if (isSlow) {
        // Laid out as paint() does it in the slow state, where the reason,
        // the bar and the buttons push the mark up.
        if (m_measured != m_reason) {
            QImage scratch(1, 1, QImage::Format_ARGB32_Premultiplied);
            QPainter p(&scratch);
            m_measuredHeight = drawParagraph(p, m_reason, font(14), 440, 14 * 1.55, QPointF(), false);
            m_measured = m_reason;
        }
        contentHeight = 150 - 40 + 26 + 27 + 10 + m_measuredHeight + 26 + 34 + (m_percent >= 0 ? 4 + 8 + 15 + 26 : 0);
    }
    const double bodyHeight = size.height() - titleBar;
    const double markTop = titleBar + (bodyHeight - contentHeight) / 2 - 40;
    const QRectF mark = QRectF(size.width() / 2 - 75, markTop, 150, 150).adjusted(-24, -24, 24, 24);
    if (isSlow) {
        // Only the light moves; the words and buttons stay as painted.
        return mark;
    }
    const QRectF status(size.width() / 2 - 200, markTop + 150 + 26 + 27 + 10, 400, 20);
    return mark.united(status);
}
