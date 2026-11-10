#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

// What the keyboard and mouse lighting shows, and the openrgb command that
// shows it, kept apart from processes and D-Bus so it can be tested alone.
// The lighting copies the taskbar underglow: the same colour and effect from
// wildosrc [Underglow], on the same rules as wildos-shell's
// underglowsettings.cpp, which the two packages cannot share.
namespace Plan
{
enum class Look {
    Colour,
    Breathing,
    Spectrum,
    // The spectrum standing still: RGB set to steady, or on battery or
    // with Reduce motion on.
    FrozenSpectrum,
};

// The spectrum's stops, as the board's RGB gradient has them.
QList<QColor> spectrum();

QColor parseAccent(const QString &value);

// The colour the devices take for accent or custom. A custom value that is
// not #rrggbb falls back to the accent, as on the taskbar.
QColor colourFor(const QString &colour, const QString &custom, const QColor &accent);

// The underglow's effect after the steady fallbacks. Full screen is left
// out: the devices run their effect themselves, which costs a game nothing.
Look look(const QString &colour, const QString &effect, bool onBattery, bool reducedMotion);

struct Device {
    int index = -1;
    QString name;
    QStringList modes;
    int leds = 0;
};

// Reads `openrgb --list-detailed`.
QList<Device> parseDevices(const QString &listing);

// One line of that listing split into names, where a name with spaces is
// in single quotes.
QStringList splitNames(const QString &line);

// The openrgb arguments that set every device to `look`. A device that
// offers none of the modes the look needs is left as it is.
QStringList arguments(const QList<Device> &devices, Look look, const QColor &colour);
}
