#pragma once

#include <QColor>
#include <QList>
#include <QString>

// The rules behind the taskbar underglow (Settings > Personalisation >
// Taskbar underglow), kept apart from KConfig and D-Bus so they can be
// tested on their own. Settings writes wildosrc [Underglow] as plain strings;
// anything it does not recognise falls back to the default rather than to no
// glow at all.
namespace UnderglowSettings
{
enum class Colour {
    Accent,
    Custom,
    Rgb,
};

enum class Effect {
    Steady,
    Breathing,
    Flowing,
};

struct Settings {
    bool enabled = false;
    Colour colour = Colour::Accent;
    // Only valid when colour is Custom.
    QColor custom;
    Effect effect = Effect::Steady;
};

// A single colour cannot flow and the spectrum does not breathe (the board
// pairs accent and custom with steady or breathing, RGB with flowing), so an
// effect that does not suit the colour becomes the one that does. A custom
// colour that is not #rrggbb falls back to the accent.
Settings parse(bool enabled, const QString &colour, const QString &custom, const QString &effect);

// The effect to run now. It holds steady on battery, while an app is full
// screen and when Reduce motion is on.
Effect running(const Settings &settings, bool onBattery, bool fullScreen, bool reducedMotion);

// The board's glow is box-shadow: 0 0 14px 2px around the 2px line. CSS
// draws that as the line grown by the 2px spread and blurred with a Gaussian
// whose standard deviation is half the 14px blur. This gives the shadow's
// coverage (0 to 1) for each one-pixel row of a band reaching `reach` pixels
// above and below the line, top to bottom, line rows included.
QList<qreal> glowRows(int reach);

QString effectName(Effect effect);
QString colourName(Colour colour);
}
