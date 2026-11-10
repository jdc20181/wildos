#include "plan.h"

#include <QTest>

using namespace Plan;

namespace
{
// The shape `openrgb --list-detailed` prints (cli.cpp, OptionListDevices).
const QString listing = QStringLiteral(
    "0: Corsair K70 RGB PRO\n"
    "  Type:           Keyboard\n"
    "  Description:    Corsair Peripheral Device\n"
    "  Modes: [Direct] Static Breathing 'Rainbow Wave' 'Spectrum Cycle'\n"
    "  Zones: Keyboard\n"
    "  LEDs: 'Key: Escape' 'Key: F1' 'Key: F2' 'Key: F3'\n"
    "\n"
    "1: Logitech G203\n"
    "  Type:           Mouse\n"
    "  Modes: Off Static ['Spectrum Cycle'] Breathing\n"
    "  LEDs: Logo\n"
    "\n"
    "2: Odd Lights\n"
    "  Modes: Direct\n"
    "  LEDs: A B C D E F G H\n"
    "\n"
    "3: Fixed Glow\n"
    "  Modes: Custom\n");
}

class PlanTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void splitsQuotedNames()
    {
        QCOMPARE(splitNames(QStringLiteral(" Direct 'Rainbow Wave'  Static")),
                 QStringList({QStringLiteral("Direct"), QStringLiteral("Rainbow Wave"), QStringLiteral("Static")}));
        QCOMPARE(splitNames(QString()), QStringList());
    }

    void parsesDevices()
    {
        const QList<Device> devices = parseDevices(listing);
        QCOMPARE(devices.size(), 4);
        QCOMPARE(devices[0].index, 0);
        QCOMPARE(devices[0].name, QStringLiteral("Corsair K70 RGB PRO"));
        QCOMPARE(devices[0].modes,
                 QStringList({QStringLiteral("Direct"), QStringLiteral("Static"), QStringLiteral("Breathing"), QStringLiteral("Rainbow Wave"),
                              QStringLiteral("Spectrum Cycle")}));
        QCOMPARE(devices[0].leds, 4);
        QCOMPARE(devices[1].modes,
                 QStringList({QStringLiteral("Off"), QStringLiteral("Static"), QStringLiteral("Spectrum Cycle"), QStringLiteral("Breathing")}));
        QCOMPARE(devices[1].leds, 1);
        QCOMPARE(devices[2].leds, 8);
        QCOMPARE(devices[3].leds, 0);
    }

    void looks_data()
    {
        QTest::addColumn<QString>("colour");
        QTest::addColumn<QString>("effect");
        QTest::addColumn<bool>("battery");
        QTest::addColumn<bool>("reduced");
        QTest::addColumn<int>("expected");
        QTest::newRow("accent steady") << "accent" << "steady" << false << false << int(Look::Colour);
        QTest::newRow("custom breathing") << "custom" << "breathing" << false << false << int(Look::Breathing);
        QTest::newRow("breathing on battery") << "accent" << "breathing" << true << false << int(Look::Colour);
        QTest::newRow("breathing reduced") << "accent" << "breathing" << false << true << int(Look::Colour);
        QTest::newRow("accent flowing") << "accent" << "flowing" << false << false << int(Look::Colour);
        QTest::newRow("rgb") << "rgb" << "flowing" << false << false << int(Look::Spectrum);
        QTest::newRow("rgb no effect") << "rgb" << "" << false << false << int(Look::Spectrum);
        QTest::newRow("rgb steady") << "rgb" << "steady" << false << false << int(Look::FrozenSpectrum);
        QTest::newRow("rgb on battery") << "rgb" << "flowing" << true << false << int(Look::FrozenSpectrum);
    }

    void looks()
    {
        QFETCH(QString, colour);
        QFETCH(QString, effect);
        QFETCH(bool, battery);
        QFETCH(bool, reduced);
        QFETCH(int, expected);
        QCOMPARE(int(look(colour, effect, battery, reduced)), expected);
    }

    void colours()
    {
        const QColor accent(0x3d, 0xff, 0x7a);
        QCOMPARE(colourFor(QStringLiteral("custom"), QStringLiteral("#ff2d95"), accent), QColor(0xff, 0x2d, 0x95));
        QCOMPARE(colourFor(QStringLiteral("custom"), QStringLiteral("pink"), accent), accent);
        QCOMPARE(colourFor(QStringLiteral("accent"), QStringLiteral("#ff2d95"), accent), accent);
        QCOMPARE(parseAccent(QStringLiteral("61,255,122")), accent);
        QCOMPARE(parseAccent(QStringLiteral("#3dff7a")), accent);
        QCOMPARE(parseAccent(QString()), QColor(0x8f, 0xb4, 0xff));
    }

    void breathingPrefersBreathingMode()
    {
        const QStringList args = arguments(parseDevices(listing), Look::Breathing, QColor(0xff, 0x2d, 0x95));
        QCOMPARE(args,
                 QStringList({"--device", "0", "--mode", "Breathing", "--color", "ff2d95",
                              "--device", "1", "--mode", "Breathing", "--color", "ff2d95",
                              "--device", "2", "--mode", "Direct", "--color", "ff2d95"}));
    }

    void colourPrefersStatic()
    {
        const QStringList args = arguments(parseDevices(listing), Look::Colour, QColor(0x3d, 0xff, 0x7a));
        QCOMPARE(args,
                 QStringList({"--device", "0", "--mode", "Static", "--color", "3dff7a",
                              "--device", "1", "--mode", "Static", "--color", "3dff7a",
                              "--device", "2", "--mode", "Direct", "--color", "3dff7a"}));
    }

    void spectrumUsesDeviceEffects()
    {
        const QStringList args = arguments(parseDevices(listing), Look::Spectrum, QColor());
        QCOMPARE(args.mid(0, 4), QStringList({"--device", "0", "--mode", "Rainbow Wave"}));
        QCOMPARE(args.mid(4, 4), QStringList({"--device", "1", "--mode", "Spectrum Cycle"}));
        // No rainbow mode: the spectrum laid across the LEDs.
        QCOMPARE(args.mid(8, 4), QStringList({"--device", "2", "--mode", "Direct"}));
        QCOMPARE(args.at(12), QStringLiteral("--color"));
        const QStringList colours = args.at(13).split(QLatin1Char(','));
        QCOMPARE(colours.size(), 8);
        QCOMPARE(colours.first(), QStringLiteral("ff3d6e"));
        QCOMPARE(args.size(), 14);
    }

    void frozenSpectrum()
    {
        const QStringList args = arguments(parseDevices(listing), Look::FrozenSpectrum, QColor());
        // Four LEDs: a quarter of the way along the gradient each.
        QCOMPARE(args.mid(0, 5), QStringList({"--device", "0", "--mode", "Direct", "--color"}));
        QCOMPARE(args.at(5).split(QLatin1Char(',')).size(), 4);
        // One LED and no Direct: the spectrum's middle colour.
        QCOMPARE(args.mid(6, 6), QStringList({"--device", "1", "--mode", "Static", "--color", "3dd5ff"}));
    }
};

QTEST_GUILESS_MAIN(PlanTest)

#include "plantest.moc"
