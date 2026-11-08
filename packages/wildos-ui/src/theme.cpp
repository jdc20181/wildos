#include "theme.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QEvent>
#include <QFont>
#include <QGuiApplication>

namespace
{
// Glacier, for a session that has never picked an accent (the greeter, a
// fresh account before the welcome app runs).
const QColor glacier(0x8f, 0xb4, 0xff);

QColor parseColour(const QString &value)
{
    // The colours module writes "r,g,b"; older tools wrote a #hex name.
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() >= 3) {
        return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
    }
    return value.isEmpty() ? QColor() : QColor(value);
}
}

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    read();
    qGuiApp->installEventFilter(this);
    // plasma-apply-colorscheme rewrites kdeglobals in several steps, and
    // between them the accent entry is briefly gone. Reading at once would
    // flash the default accent before the new one, so changes are read once
    // the burst has settled.
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);
    connect(&m_settle, &QTimer::timeout, this, &Theme::read);
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    if (m_watcher) {
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() == QLatin1String("General") || group.name() == QLatin1String("KDE")
                || group.name().startsWith(QLatin1String("Colors:"))) {
                m_settle.start();
            }
        });
    }
}

void Theme::read()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    const KConfigGroup general = config->group(QStringLiteral("General"));

    QColor colour = parseColour(general.readEntry("AccentColor"));
    if (!colour.isValid()) {
        colour = glacier;
    }
    if (colour != m_accent) {
        m_accent = colour;
        Q_EMIT accentChanged();
    }

    // Judged by the window colour of the scheme applied, the same rule the
    // Wild OS Kvantum theme and title bars follow, so every part of the
    // desktop agrees. The scheme's name is only a label and can be missing
    // while its colours are there; it is the fallback when they are not.
    // The variable is for previews and the gallery.
    const QString scheme = general.readEntry("ColorScheme", QStringLiteral("WildOS"));
    bool dark = true;
    const QByteArray forced = qgetenv("WILDOS_UI_THEME");
    const QColor window = parseColour(config->group(QStringLiteral("Colors:Window")).readEntry("BackgroundNormal"));
    if (forced == "light" || forced == "dark") {
        dark = forced == "dark";
    } else if (window.isValid()) {
        dark = window.lightnessF() < 0.5;
    } else {
        dark = !scheme.contains(QLatin1String("Light"));
    }
    if (dark != m_dark || scheme != m_scheme) {
        m_dark = dark;
        m_scheme = scheme;
        Q_EMIT darkChanged();
    }

    const bool reduced = config->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0) <= 0.0;
    if (reduced != m_reducedMotion) {
        m_reducedMotion = reduced;
        Q_EMIT reducedMotionChanged();
    }
}

qreal Theme::fontPointSize() const
{
    // A font set in pixels reports no point size; 96 dpi is what the
    // platform theme assumes for the conversion too.
    const QFont font = QGuiApplication::font();
    return font.pointSizeF() > 0 ? font.pointSizeF() : font.pixelSize() * 0.75;
}

bool Theme::eventFilter(QObject *watched, QEvent *event)
{
    // The platform theme sets a new application font when Settings changes
    // the text size; every window then receives this.
    if (watched == qGuiApp && event->type() == QEvent::ApplicationFontChange) {
        Q_EMIT fontPointSizeChanged();
    }
    return QObject::eventFilter(watched, event);
}
