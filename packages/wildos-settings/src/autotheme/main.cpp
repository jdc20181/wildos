// wildos-auto-theme: switches between the light and dark themes at sunrise
// and sunset while Settings' theme is Automatic, and keeps the window title
// bars and the Plasma style (the glass of Start and the flyouts) in step
// with the colour scheme, the theme and stronger text on glass however they
// were changed (Settings, Welcome, the switch here). Started with every
// session.

#include "sun.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

#include <QColor>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace
{
const QString darkScheme = QStringLiteral("WildOS");
const QString lightScheme = QStringLiteral("WildOSLight");
const QString contrastScheme = QStringLiteral("WildOSHighContrast");

const QString decorationGroup = QStringLiteral("org.kde.kdecoration2");
const QString darkDecoration = QStringLiteral("__aurorae__svg__WildOS");
const QString lightDecoration = QStringLiteral("__aurorae__svg__WildOSLight");

const QString darkStyle = QStringLiteral("wildos");
const QString lightStyle = QStringLiteral("wildos-light");

bool hasStyle(const QString &name)
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("plasma/desktoptheme/%1/metadata.json").arg(name)).isEmpty();
}

// Light or dark goes by the window colour of the scheme applied, the rule
// the Wild OS apps and the Kvantum theme use: the scheme's name is only a
// label, and can be missing while its colours are there.
bool schemeIsLight(const KSharedConfigPtr &globals)
{
    const QStringList rgb = globals->group(QStringLiteral("Colors:Window")).readEntry("BackgroundNormal").split(QLatin1Char(','));
    if (rgb.size() >= 3) {
        return QColor(rgb[0].trimmed().toInt(), rgb[1].trimmed().toInt(), rgb[2].trimmed().toInt()).lightnessF() >= 0.5;
    }
    return globals->group(QStringLiteral("General")).readEntry("ColorScheme").contains(QLatin1String("Light"));
}

bool automatic()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    return config->group(QStringLiteral("Theme")).readEntry("Mode") == QLatin1String("automatic");
}

class Switcher : public QObject
{
    Q_OBJECT

public:
    Switcher()
    {
        m_wildos = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc")));
        connect(m_wildos.data(), &KConfigWatcher::configChanged, this, &Switcher::check);
        connect(m_wildos.data(), &KConfigWatcher::configChanged, this, &Switcher::matchStyle);
        m_globals = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
        connect(m_globals.data(), &KConfigWatcher::configChanged, this, &Switcher::matchDecoration);
        connect(m_globals.data(), &KConfigWatcher::configChanged, this, &Switcher::matchStyle);

        // Checking once a minute also covers waking from sleep, a changed
        // time zone and a clock set by hand, which a timer aimed at the next
        // sunset would miss.
        m_tick.setInterval(60 * 1000);
        m_tick.setTimerType(Qt::VeryCoarseTimer);
        connect(&m_tick, &QTimer::timeout, this, &Switcher::check);
        // Also a backstop for a scheme written without telling anyone.
        connect(&m_tick, &QTimer::timeout, this, &Switcher::matchDecoration);
        connect(&m_tick, &QTimer::timeout, this, &Switcher::matchStyle);
        m_tick.start();

        QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                                             QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"), this,
                                             SLOT(check()));
        check();
        matchDecoration();
        matchStyle();
    }

    // Start, the flyouts and tooltips are drawn by the Plasma style, whose
    // glass is fixed in its frames, so each theme glass has a style of its
    // own (wildos-branding builds them) and the style follows the theme and
    // the mode: the theme's while the session is in the theme's mode, the
    // plain dark or light one otherwise, and its solid twin while stronger
    // text on glass is on. A style someone picked themselves is left alone.
    Q_SLOT void matchStyle()
    {
        if (m_styling) {
            // Asked again while a style goes on (stronger text switched on
            // and off quickly): look again once it is in.
            m_restyle = true;
            return;
        }
        KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        globals->reparseConfiguration();
        const bool light = schemeIsLight(globals) && globals->group(QStringLiteral("General")).readEntry("ColorScheme") != contrastScheme;

        QString wanted = light && hasStyle(lightStyle) ? lightStyle : darkStyle;
        KSharedConfigPtr wildos = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
        wildos->reparseConfiguration();
        const QString id = wildos->group(QStringLiteral("Theme")).readEntry("Id", QStringLiteral("dawn"));
        QFile file(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("wildos/themes/%1.json").arg(id)));
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject theme = QJsonDocument::fromJson(file.readAll()).object();
            const QString style = theme.value(QStringLiteral("desktopTheme")).toString();
            if ((theme.value(QStringLiteral("mode")).toString() == QLatin1String("light")) == light && !style.isEmpty() && hasStyle(style)) {
                wanted = style;
            }
        }
        // Settings > Accessibility > Stronger text on glass: the same style
        // with its glass made nearly solid.
        if (wildos->group(QStringLiteral("Accessibility")).readEntry("StrongerText", false) && hasStyle(wanted + QStringLiteral("-solid"))) {
            wanted += QStringLiteral("-solid");
        }

        KSharedConfigPtr plasmarc = KSharedConfig::openConfig(QStringLiteral("plasmarc"));
        plasmarc->reparseConfiguration();
        const QString current = plasmarc->group(QStringLiteral("Theme")).readEntry("name", darkStyle);
        if (current == wanted || (current != darkStyle && !current.startsWith(darkStyle + QLatin1Char('-')))) {
            return;
        }
        m_styling = true;
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            m_styling = false;
            if (m_restyle) {
                m_restyle = false;
                matchStyle();
            }
        });
        process->start(QStringLiteral("plasma-apply-desktoptheme"), {wanted});
    }

    // The title bar is drawn by KWin from an Aurorae theme with fixed
    // colours, one for each scheme, so it is swapped with the scheme. A
    // decoration someone picked themselves is left alone.
    Q_SLOT void matchDecoration()
    {
        KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        globals->reparseConfiguration();
        const QString wanted = schemeIsLight(globals) ? lightDecoration : darkDecoration;
        KSharedConfigPtr kwin = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        kwin->reparseConfiguration();
        KConfigGroup decoration = kwin->group(decorationGroup);
        const QString current = decoration.readEntry("theme", darkDecoration);
        if (current == wanted || (current != darkDecoration && current != lightDecoration)) {
            return;
        }
        decoration.writeEntry("theme", wanted);
        kwin->sync();
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure")));
    }

    Q_SLOT void check()
    {
        if (m_applying || !automatic()) {
            return;
        }
        const QString wanted = Sun::isLight(QDateTime::currentDateTime()) ? lightScheme : darkScheme;
        if (QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(wanted)).isEmpty()) {
            return;
        }
        KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        globals->reparseConfiguration();
        // High contrast (Settings > Accessibility) is not a time of day
        // choice; it stays until it is turned off, which brings the
        // automatic theme back.
        if (globals->group(QStringLiteral("General")).readEntry("ColorScheme") == contrastScheme) {
            return;
        }
        if (schemeIsLight(globals) == (wanted == lightScheme)) {
            return;
        }
        apply(wanted, globals->group(QStringLiteral("General")).readEntry("AccentColor"));
    }

private:
    // As Settings does it: the tool resets the accent key, so the accent
    // goes back on afterwards with the tool's own accent option, and then
    // the key Wild OS widgets read.
    void apply(const QString &scheme, const QString &accent)
    {
        m_applying = true;
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, accent] {
            process->deleteLater();
            if (accent.isEmpty()) {
                m_applying = false;
                return;
            }
            const QStringList rgb = accent.split(QLatin1Char(','));
            const QColor colour = rgb.size() >= 3 ? QColor(rgb[0].trimmed().toInt(), rgb[1].trimmed().toInt(), rgb[2].trimmed().toInt()) : QColor(accent);
            auto restore = new QProcess(this);
            connect(restore, &QProcess::finished, this, [this, restore, accent] {
                restore->deleteLater();
                KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
                globals->reparseConfiguration();
                KConfigGroup general = globals->group(QStringLiteral("General"));
                general.writeEntry("AccentColor", accent, KConfig::Notify);
                general.writeEntry("LastUsedCustomAccentColor", accent, KConfig::Notify);
                globals->sync();
                m_applying = false;
            });
            restore->start(QStringLiteral("plasma-apply-colorscheme"), {QStringLiteral("--accent-color"), colour.name(QColor::HexRgb)});
        });
        process->start(QStringLiteral("plasma-apply-colorscheme"), {scheme});
    }

    KConfigWatcher::Ptr m_wildos;
    KConfigWatcher::Ptr m_globals;
    QTimer m_tick;
    bool m_applying = false;
    bool m_styling = false;
    bool m_restyle = false;
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-auto-theme"));
    Switcher switcher;
    return app.exec();
}

#include "main.moc"
