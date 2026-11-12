#include "screens.h"
#include "wayland.h"

#include <QGuiApplication>
#include <QScreen>

#include <cmath>

Screens::Screens(QObject *parent)
    : QObject(parent)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(500);
    connect(&m_settle, &QTimer::timeout, this, &Screens::changed);
    for (QScreen *s : QGuiApplication::screens()) {
        watch(s);
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *s) {
        watch(s);
        m_settle.start();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { m_settle.start(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { m_settle.start(); });
    WaylandExtras::instance().onOutputsChanged([this] { m_settle.start(); });
}

void Screens::watch(QScreen *screen)
{
    connect(screen, &QScreen::geometryChanged, this, [this] { m_settle.start(); });
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this] { m_settle.start(); });
}

QList<Monitor> Screens::layout(double fallback) const
{
    QList<QScreen *> screens = QGuiApplication::screens();
    if (QScreen *primary = QGuiApplication::primaryScreen(); primary && screens.removeOne(primary)) {
        screens.prepend(primary);
    }
    const QList<OutputInfo> outputs = WaylandExtras::instance().outputs();
    auto outputFor = [&](const QScreen *s) -> const OutputInfo * {
        for (const OutputInfo &o : outputs) {
            if (o.name == s->name()) {
                return &o;
            }
        }
        return nullptr;
    };
    // Every screen's own scale, or none: a layout that mixes exact scales
    // with guesses would put some windows at the wrong size.
    bool exact = qEnvironmentVariable("WILDOS_RDP_MONITORS") != QLatin1String("uniform") && !screens.isEmpty();
    for (const QScreen *s : std::as_const(screens)) {
        exact = exact && outputFor(s);
    }
    QList<Monitor> out;
    for (const QScreen *s : std::as_const(screens)) {
        Monitor m;
        m.name = s->name();
        m.logical = s->geometry();
        m.millimetres = s->physicalSize().toSize();
        m.scale = fallback > 0 ? fallback : 1.0;
        if (exact) {
            const OutputInfo *o = outputFor(s);
            m.scale = o->scale;
            // The mode, unless it does not match the screen's size here (a
            // description not yet caught up with a change).
            const QSize expected(int(std::lround(m.logical.width() * m.scale)), int(std::lround(m.logical.height() * m.scale)));
            if (std::abs(o->pixels.width() - expected.width()) <= m.scale + 1 && std::abs(o->pixels.height() - expected.height()) <= m.scale + 1) {
                m.pixels = o->pixels;
            }
        }
        out.append(m);
    }
    return monitors::arrange(out);
}
