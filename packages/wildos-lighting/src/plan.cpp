#include "plan.h"

#include <QRegularExpression>

namespace Plan
{
namespace
{
// Glacier, as everywhere in Wild OS before an accent is picked.
const QColor glacier(0x8f, 0xb4, 0xff);

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb).mid(1);
}

// The first of `wanted` the device offers, in the device's own spelling.
QString pick(const QStringList &modes, const QStringList &wanted)
{
    for (const QString &w : wanted) {
        for (const QString &m : modes) {
            if (m.compare(w, Qt::CaseInsensitive) == 0) {
                return m;
            }
        }
    }
    return QString();
}

QColor spectrumAt(qreal t)
{
    const QList<QColor> stops = spectrum();
    const qreal scaled = t * (stops.size() - 1);
    const int i = qBound(0, int(scaled), int(stops.size()) - 2);
    const qreal f = scaled - i;
    const QColor a = stops[i];
    const QColor b = stops[i + 1];
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f);
}
}

QList<QColor> spectrum()
{
    return {QColor(0xff, 0x3d, 0x6e), QColor(0xff, 0xb0, 0x3d), QColor(0xf5, 0xff, 0x3d), QColor(0x3d, 0xff, 0x7a),
            QColor(0x3d, 0xd5, 0xff), QColor(0x5c, 0x7c, 0xff), QColor(0xc8, 0x5c, 0xff), QColor(0xff, 0x3d, 0x6e)};
}

QColor parseAccent(const QString &value)
{
    // The colours module writes "r,g,b"; older tools wrote a #hex name.
    const QStringList parts = value.split(QLatin1Char(','));
    QColor colour;
    if (parts.size() >= 3) {
        colour = QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
    } else if (!value.isEmpty()) {
        colour = QColor(value.trimmed());
    }
    return colour.isValid() ? colour : glacier;
}

QColor colourFor(const QString &colour, const QString &custom, const QColor &accent)
{
    static const QRegularExpression rrggbb(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    if (colour.trimmed().compare(QLatin1String("custom"), Qt::CaseInsensitive) == 0 && rrggbb.match(custom.trimmed()).hasMatch()) {
        return QColor(custom.trimmed());
    }
    return accent;
}

Look look(const QString &colour, const QString &effect, bool onBattery, bool reducedMotion)
{
    const bool still = onBattery || reducedMotion;
    const QString e = effect.trimmed().toLower();
    if (colour.trimmed().compare(QLatin1String("rgb"), Qt::CaseInsensitive) == 0) {
        return still || e == QLatin1String("steady") ? Look::FrozenSpectrum : Look::Spectrum;
    }
    return !still && e == QLatin1String("breathing") ? Look::Breathing : Look::Colour;
}

QStringList splitNames(const QString &line)
{
    QStringList names;
    QString current;
    bool quoted = false;
    bool inName = false;
    for (const QChar c : line) {
        if (quoted) {
            if (c == QLatin1Char('\'')) {
                quoted = false;
            } else {
                current += c;
            }
        } else if (c == QLatin1Char(' ')) {
            if (inName) {
                names.append(current);
                current.clear();
                inName = false;
            }
        } else if (c == QLatin1Char('\'') && !inName) {
            quoted = true;
            inName = true;
        } else {
            current += c;
            inName = true;
        }
    }
    if (inName) {
        names.append(current);
    }
    return names;
}

QList<Device> parseDevices(const QString &listing)
{
    static const QRegularExpression header(QStringLiteral("^(\\d+): (.*)$"));
    QList<Device> devices;
    for (const QString &line : listing.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = header.match(line);
        if (m.hasMatch()) {
            Device d;
            d.index = m.captured(1).toInt();
            d.name = m.captured(2).trimmed();
            devices.append(d);
            continue;
        }
        if (devices.isEmpty()) {
            continue;
        }
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("Modes:"))) {
            // The active mode is shown in brackets, outside the quotes of a
            // name with spaces: ['Spectrum Cycle']. Moving the brackets
            // inside lets the name split like the others.
            QString rest = trimmed.mid(6);
            rest.replace(QLatin1String("['"), QLatin1String("'["));
            rest.replace(QLatin1String("']"), QLatin1String("]'"));
            QStringList modes = splitNames(rest);
            for (QString &mode : modes) {
                if (mode.startsWith(QLatin1Char('[')) && mode.endsWith(QLatin1Char(']'))) {
                    mode = mode.mid(1, mode.size() - 2);
                }
            }
            devices.last().modes = modes;
        } else if (trimmed.startsWith(QLatin1String("LEDs:"))) {
            devices.last().leds = splitNames(trimmed.mid(5)).size();
        }
    }
    return devices;
}

QStringList arguments(const QList<Device> &devices, Look look, const QColor &colour)
{
    static const QStringList solid = {QStringLiteral("Static"), QStringLiteral("Direct")};
    static const QStringList breathing = {QStringLiteral("Breathing")};
    static const QStringList flowing = {QStringLiteral("Rainbow Wave"), QStringLiteral("Rainbow"), QStringLiteral("Spectrum Cycle"),
                                        QStringLiteral("Wave")};

    QStringList args;
    for (const Device &d : devices) {
        const auto set = [&args, &d](const QString &mode, const QString &colours) {
            args << QStringLiteral("--device") << QString::number(d.index) << QStringLiteral("--mode") << mode;
            if (!colours.isEmpty()) {
                args << QStringLiteral("--color") << colours;
            }
        };

        if (look == Look::Spectrum) {
            const QString mode = pick(d.modes, flowing);
            if (!mode.isEmpty()) {
                set(mode, QString());
                continue;
            }
        }
        if (look == Look::Spectrum || look == Look::FrozenSpectrum) {
            // Direct takes a colour per LED, so the spectrum can be laid
            // across the device. A device that can only show one colour
            // takes the spectrum's middle.
            const QString direct = pick(d.modes, {QStringLiteral("Direct")});
            if (!direct.isEmpty() && d.leds > 1) {
                QStringList colours;
                for (int i = 0; i < d.leds; ++i) {
                    colours << hex(spectrumAt(qreal(i) / d.leds));
                }
                set(direct, colours.join(QLatin1Char(',')));
                continue;
            }
            const QString mode = pick(d.modes, solid);
            if (!mode.isEmpty()) {
                set(mode, hex(spectrum().at(4)));
            }
            continue;
        }
        if (look == Look::Breathing) {
            const QString mode = pick(d.modes, breathing);
            if (!mode.isEmpty()) {
                set(mode, hex(colour));
                continue;
            }
        }
        const QString mode = pick(d.modes, solid);
        if (!mode.isEmpty()) {
            set(mode, hex(colour));
        }
    }
    return args;
}
}
