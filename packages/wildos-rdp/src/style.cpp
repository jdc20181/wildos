#include "style.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace
{
const QColor glacier(0x8f, 0xb4, 0xff);
const QColor darkReference = QColor::fromRgbF(18 / 255.0, 22 / 255.0, 40 / 255.0, 0.86);
const QColor lightReference = QColor::fromRgbF(250 / 255.0, 251 / 255.0, 254 / 255.0, 0.82);

QColor parseColour(const QString &value)
{
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() >= 3) {
        return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
    }
    return value.isEmpty() ? QColor() : QColor(value);
}

QColor jsonColour(const QJsonValue &value)
{
    if (value.isString()) {
        return QColor(value.toString());
    }
    const QJsonObject object = value.toObject();
    QColor colour(object.value(QStringLiteral("colour")).toString());
    if (colour.isValid()) {
        colour.setAlphaF(object.value(QStringLiteral("alpha")).toDouble(1.0));
    }
    return colour;
}

// Style.tinted: each channel scaled by the theme's share of the reference
// glass, the alpha moved by the theme's difference.
QColor tinted(const QColor &tint, const QColor &ref, int r, int g, int b, double a)
{
    return QColor::fromRgbF(std::min(1.0, r / 255.0 * tint.redF() / ref.redF()), std::min(1.0, g / 255.0 * tint.greenF() / ref.greenF()),
                            std::min(1.0, b / 255.0 * tint.blueF() / ref.blueF()),
                            std::clamp(a + tint.alphaF() - ref.alphaF(), 0.0, 1.0));
}
}

Style Style::read()
{
    Style s;
    KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    const KConfigGroup general = globals->group(QStringLiteral("General"));
    s.accent = parseColour(general.readEntry("AccentColor"));
    if (!s.accent.isValid()) {
        s.accent = glacier;
    }
    const QString scheme = general.readEntry("ColorScheme", QStringLiteral("WildOS"));
    const QColor window = parseColour(globals->group(QStringLiteral("Colors:Window")).readEntry("BackgroundNormal"));
    s.contrast = scheme == QLatin1String("WildOSHighContrast");
    if (s.contrast) {
        s.dark = true;
    } else if (window.isValid()) {
        s.dark = window.lightnessF() < 0.5;
    } else {
        s.dark = !scheme.contains(QLatin1String("Light"));
    }
    s.reducedMotion = globals->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0) <= 0.0;

    const QString id = KSharedConfig::openConfig(QStringLiteral("wildosrc"))->group(QStringLiteral("Theme")).readEntry("Id", QStringLiteral("dawn"));
    QJsonObject theme;
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("wildos/themes/%1.json").arg(id));
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
        theme = QJsonDocument::fromJson(file.readAll()).object();
    }
    const QString mode = theme.value(QStringLiteral("mode")).toString();
    const QColor themeGlass = jsonColour(theme.value(QStringLiteral("glass")));
    const QColor ref = s.dark ? darkReference : lightReference;
    const bool themed = !s.contrast && !mode.isEmpty() && (mode == QLatin1String("dark")) == s.dark && themeGlass.isValid();
    const QColor tint = themed ? themeGlass : ref;

    if (s.contrast) {
        s.glass = Qt::black;
        s.border = Qt::white;
        s.ink = Qt::white;
    } else if (s.dark) {
        s.glass = tinted(tint, ref, 20, 24, 42, 0.80);
        s.border = QColor::fromRgbF(1, 1, 1, 0.12);
        s.ink = QColor(0xee, 0xf1, 0xf8);
    } else {
        s.glass = tinted(tint, ref, 250, 251, 254, 0.92);
        s.border = QColor::fromRgbF(30 / 255.0, 40 / 255.0, 70 / 255.0, 0.12 * 0.75);
        s.ink = QColor(0x15, 0x1b, 0x2c);
    }
    return s;
}
