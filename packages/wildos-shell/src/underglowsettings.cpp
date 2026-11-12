#include "underglowsettings.h"

#include <QRegularExpression>

#include <cmath>

namespace UnderglowSettings
{

Settings parse(bool enabled, const QString &colour, const QString &custom, const QString &effect)
{
    Settings s;
    s.enabled = enabled;

    const QString c = colour.trimmed().toLower();
    if (c == QLatin1String("rgb")) {
        s.colour = Colour::Rgb;
    } else if (c == QLatin1String("custom")) {
        // QColor would also take names such as "red" and #rgb shorthand;
        // Settings only ever writes #rrggbb, so anything else is a damaged
        // file, not a colour choice.
        static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        const QString value = custom.trimmed();
        if (hex.match(value).hasMatch()) {
            s.colour = Colour::Custom;
            s.custom = QColor(value);
        }
    }

    const QString e = effect.trimmed().toLower();
    if (s.colour == Colour::Rgb) {
        s.effect = e == QLatin1String("steady") ? Effect::Steady : Effect::Flowing;
    } else {
        s.effect = e == QLatin1String("breathing") ? Effect::Breathing : Effect::Steady;
    }
    return s;
}

Effect running(const Settings &settings, bool onBattery, bool fullScreen, bool reducedMotion)
{
    if (onBattery || fullScreen || reducedMotion) {
        return Effect::Steady;
    }
    return settings.effect;
}

namespace
{
constexpr int lineHeight = 2;

// Coverage of each one-pixel row of the strip for a band `height` pixels
// tall, centred on the line and blurred with a Gaussian of `sigma`.
QList<qreal> bandRows(int reach, qreal height, qreal sigma)
{
    const qreal half = height / 2;
    const auto phi = [](qreal x) {
        return 0.5 * (1 + std::erf(x / std::sqrt(2.0)));
    };

    QList<qreal> rows;
    const int count = reach * 2 + lineHeight;
    rows.reserve(count);
    const qreal centre = reach + lineHeight / 2.0;
    for (int y = 0; y < count; ++y) {
        const qreal d = y + 0.5 - centre;
        rows.append(phi((d + half) / sigma) - phi((d - half) / sigma));
    }
    return rows;
}
}

QList<qreal> glowRows(int reach)
{
    constexpr qreal spread = 2;
    constexpr qreal blur = 6;
    return bandRows(reach, lineHeight + spread * 2, blur / 2);
}

QList<qreal> spectrumCopyRows(int reach)
{
    constexpr qreal height = 6;
    constexpr qreal blur = 4;
    return bandRows(reach, height, blur);
}

qreal flowPhase(qint64 msecsSinceEpoch)
{
    return qreal(msecsSinceEpoch % flowCycleMs) / flowCycleMs;
}

QString effectName(Effect effect)
{
    switch (effect) {
    case Effect::Breathing:
        return QStringLiteral("breathing");
    case Effect::Flowing:
        return QStringLiteral("flowing");
    case Effect::Steady:
        break;
    }
    return QStringLiteral("steady");
}

QString colourName(Colour colour)
{
    switch (colour) {
    case Colour::Custom:
        return QStringLiteral("custom");
    case Colour::Rgb:
        return QStringLiteral("rgb");
    case Colour::Accent:
        break;
    }
    return QStringLiteral("accent");
}

}
