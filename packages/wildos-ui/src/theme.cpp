#include "theme.h"

#include "historyswipe.h"

#include <KConfigGroup>
#include <algorithm>
#include <KSharedConfig>

#include <QEvent>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

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

// Dawn's Mica window glass in each scheme. A theme's
// glass is given as this one surface; every other surface keeps its
// relation to it (see Style.tinted).
const QColor darkReference = QColor::fromRgbF(24 / 255.0, 26 / 255.0, 34 / 255.0, 0.95);
const QColor lightReference = QColor::fromRgbF(243 / 255.0, 243 / 255.0, 243 / 255.0, 0.96);

// {"colour": "#rrggbb", "alpha": 0.86}, or a bare "#rrggbb".
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
}

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    read();
    qGuiApp->installEventFilter(this);
    HistorySwipe::install();
    // plasma-apply-colorscheme rewrites kdeglobals in several steps, and
    // between them the accent entry is briefly gone. Reading at once would
    // flash the default accent before the new one, so changes are read once
    // the burst has settled.
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);
    connect(&m_settle, &QTimer::timeout, this, &Theme::read);
    // Whether the theme's glass and text colours apply depends on the mode
    // and the accent, so either change can change them.
    connect(this, &Theme::darkChanged, this, &Theme::themeChanged);
    connect(this, &Theme::accentChanged, this, &Theme::themeChanged);
    readTheme();
    readAccessibility();
    m_wildosWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc")));
    if (m_wildosWatcher) {
        connect(m_wildosWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() == QLatin1String("Theme")) {
                readTheme();
            } else if (group.name() == QLatin1String("Accessibility")) {
                readAccessibility();
            }
        });
    }
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
    if (scheme == QLatin1String("WildOSHighContrast")) {
        dark = true;
    } else if (forced == "light" || forced == "dark") {
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

    const qreal factor = std::clamp(config->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0), 0.0, 4.0);
    const bool reduced = factor <= 0.0;
    if (reduced != m_reducedMotion || !qFuzzyCompare(1.0 + factor, 1.0 + m_animationFactor)) {
        m_reducedMotion = reduced;
        m_animationFactor = factor;
        Q_EMIT reducedMotionChanged();
    }
}

void Theme::readTheme()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const QString id = config->group(QStringLiteral("Theme")).readEntry("Id", QStringLiteral("dawn"));

    QJsonObject theme;
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("wildos/themes/%1.json").arg(id));
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
        theme = QJsonDocument::fromJson(file.readAll()).object();
    }

    m_themeId = id;
    m_themeMode = theme.value(QStringLiteral("mode")).toString();
    m_themeAccent = jsonColour(theme.value(QStringLiteral("accent")).toObject().value(QStringLiteral("colour")));
    m_glass = jsonColour(theme.value(QStringLiteral("glass")));
    m_taskbarTint = jsonColour(theme.value(QStringLiteral("taskbarGlass")));
    m_panelTint = jsonColour(theme.value(QStringLiteral("panelGlass")));
    m_textOnAccent = jsonColour(theme.value(QStringLiteral("textOnAccent")));
    m_accentText = jsonColour(theme.value(QStringLiteral("accentText")));
    Q_EMIT themeChanged();
}

void Theme::readAccessibility()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const bool stronger = config->group(QStringLiteral("Accessibility")).readEntry("StrongerText", false);
    if (stronger != m_strongerText) {
        m_strongerText = stronger;
        Q_EMIT strongerTextChanged();
    }
}

bool Theme::themeMode() const
{
    if (highContrast() || m_themeMode.isEmpty()) {
        return false;
    }
    return (m_themeMode == QLatin1String("dark")) == m_dark;
}

QColor Theme::glassReference() const
{
    return m_dark ? darkReference : lightReference;
}

QColor Theme::glassTint() const
{
    return themeMode() && m_glass.isValid() ? m_glass : glassReference();
}

bool Theme::themeAccent() const
{
    return themeMode() && m_themeAccent.isValid() && m_themeAccent.rgb() == m_accent.rgb() && m_textOnAccent.isValid()
        && m_accentText.isValid();
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
