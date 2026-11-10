#include "underglowsettings.h"

#include <QTest>

using namespace UnderglowSettings;

class UnderglowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        const Settings s = parse(false, QString(), QString(), QString());
        QCOMPARE(s.enabled, false);
        QCOMPARE(s.colour, Colour::Accent);
        QCOMPARE(s.effect, Effect::Steady);
    }

    void accentBreathing()
    {
        const Settings s = parse(true, QStringLiteral("accent"), QString(), QStringLiteral("breathing"));
        QCOMPARE(s.enabled, true);
        QCOMPARE(s.colour, Colour::Accent);
        QCOMPARE(s.effect, Effect::Breathing);
    }

    void customColour()
    {
        const Settings s = parse(true, QStringLiteral("custom"), QStringLiteral("#FF2D95"), QStringLiteral("steady"));
        QCOMPARE(s.colour, Colour::Custom);
        QCOMPARE(s.custom, QColor(0xff, 0x2d, 0x95));
    }

    void customNotHexFallsBackToAccent_data()
    {
        QTest::addColumn<QString>("value");
        QTest::newRow("empty") << QString();
        QTest::newRow("name") << QStringLiteral("red");
        QTest::newRow("short") << QStringLiteral("#f0f");
        QTest::newRow("alpha") << QStringLiteral("#80ff2d95");
        QTest::newRow("junk") << QStringLiteral("#ff2d9g");
    }

    void customNotHexFallsBackToAccent()
    {
        QFETCH(QString, value);
        const Settings s = parse(true, QStringLiteral("custom"), value, QStringLiteral("breathing"));
        QCOMPARE(s.colour, Colour::Accent);
        QCOMPARE(s.effect, Effect::Breathing);
    }

    void effectFollowsColour_data()
    {
        QTest::addColumn<QString>("colour");
        QTest::addColumn<QString>("effect");
        QTest::addColumn<int>("expected");
        QTest::newRow("accent flowing") << "accent" << "flowing" << int(Effect::Steady);
        QTest::newRow("accent unknown") << "accent" << "sparkle" << int(Effect::Steady);
        QTest::newRow("rgb default") << "rgb" << "" << int(Effect::Flowing);
        QTest::newRow("rgb breathing") << "rgb" << "breathing" << int(Effect::Flowing);
        QTest::newRow("rgb steady") << "rgb" << "steady" << int(Effect::Steady);
        QTest::newRow("rgb any case") << "RGB" << "Flowing" << int(Effect::Flowing);
        QTest::newRow("unknown colour") << "plaid" << "breathing" << int(Effect::Breathing);
    }

    void effectFollowsColour()
    {
        QFETCH(QString, colour);
        QFETCH(QString, effect);
        QFETCH(int, expected);
        QCOMPARE(int(parse(true, colour, QStringLiteral("#123456"), effect).effect), expected);
    }

    void fallsBackToSteady_data()
    {
        QTest::addColumn<bool>("battery");
        QTest::addColumn<bool>("fullScreen");
        QTest::addColumn<bool>("reduced");
        QTest::addColumn<int>("expected");
        QTest::newRow("free to run") << false << false << false << int(Effect::Flowing);
        QTest::newRow("battery") << true << false << false << int(Effect::Steady);
        QTest::newRow("full screen") << false << true << false << int(Effect::Steady);
        QTest::newRow("reduce motion") << false << false << true << int(Effect::Steady);
        QTest::newRow("all three") << true << true << true << int(Effect::Steady);
    }

    void fallsBackToSteady()
    {
        QFETCH(bool, battery);
        QFETCH(bool, fullScreen);
        QFETCH(bool, reduced);
        QFETCH(int, expected);
        const Settings s = parse(true, QStringLiteral("rgb"), QString(), QStringLiteral("flowing"));
        QCOMPARE(int(running(s, battery, fullScreen, reduced)), expected);
    }

    void glowMatchesCss()
    {
        const int reach = 20;
        const QList<qreal> rows = glowRows(reach);
        QCOMPARE(rows.size(), reach * 2 + 2);
        // Symmetric about the line, brightest on it.
        for (int i = 0; i < rows.size() / 2; ++i) {
            QVERIFY(qAbs(rows[i] - rows[rows.size() - 1 - i]) < 1e-9);
            QVERIFY(rows[i] <= rows[i + 1] + 1e-9);
        }
        // A row on the line, 0.5px off its centre, sees the 6px spread box
        // through a 7px sigma blur: about a third of full strength.
        QVERIFY(qAbs(rows[reach] - std::erf(2.5 / (7 * std::sqrt(2.0))) / 2 - std::erf(3.5 / (7 * std::sqrt(2.0))) / 2) < 1e-9);
        // At the band's edges the glow is under one 8-bit step even at the
        // 0.55 alpha the board uses, so the band is wide enough.
        QVERIFY(rows.first() * 0.55 * 255 < 1.0);
    }
};

QTEST_GUILESS_MAIN(UnderglowTest)

#include "underglowtest.moc"
