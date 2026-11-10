#include "themes.h"

#include "system.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

#include <csignal>
#include <unistd.h>

namespace
{
const QString rgbTool = QStringLiteral("/usr/libexec/wildos/rgb-devices");
const QString defaultTheme = QStringLiteral("dawn");
// The underglow renderer's default custom colour, Overclock's neon.
const QColor defaultCustom(0x3d, 0xff, 0x7a);

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
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

QString wallpaperName(const QString &path)
{
    const QString name = readJson(path + QStringLiteral("/metadata.json"))
                             .value(QStringLiteral("KPlugin")).toObject()
                             .value(QStringLiteral("Name")).toString();
    return name.isEmpty() ? QFileInfo(path).completeBaseName() : name;
}
}

Themes::Themes(QObject *parent)
    : QObject(parent)
{
    loadThemes();
    read();
    refreshRgbDevices();

    // Another Settings window, or the underglow pausing itself, can change
    // these too.
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc")));
    if (m_watcher) {
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            const QString name = group.name();
            if (name == QLatin1String("Theme") || name == QLatin1String("Underglow") || name == QLatin1String("Lighting")) {
                read();
            }
        });
    }
}

void Themes::loadThemes()
{
    QMap<QString, QJsonObject> found;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("wildos/themes"), QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        const auto entries = QDir(dir).entryInfoList({QStringLiteral("*.json")}, QDir::Files);
        for (const QFileInfo &info : entries) {
            // The first location wins, as for any XDG data file.
            if (!found.contains(info.completeBaseName())) {
                found.insert(info.completeBaseName(), readJson(info.absoluteFilePath()));
            }
        }
    }

    QList<QJsonObject> sorted = found.values();
    std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("order")).toInt(1000) < b.value(QStringLiteral("order")).toInt(1000);
    });

    for (const QJsonObject &theme : std::as_const(sorted)) {
        const QString id = theme.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        const QJsonObject accent = theme.value(QStringLiteral("accent")).toObject();
        QStringList more;
        const QJsonArray extra = theme.value(QStringLiteral("moreWallpapers")).toArray();
        for (const QJsonValue &path : extra) {
            if (QFileInfo::exists(path.toString())) {
                more << wallpaperName(path.toString());
            }
        }
        m_themes << QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), theme.value(QStringLiteral("name")).toString()},
            {QStringLiteral("description"), theme.value(QStringLiteral("description")).toString()},
            {QStringLiteral("mode"), theme.value(QStringLiteral("mode")).toString()},
            {QStringLiteral("accent"), jsonColour(accent.value(QStringLiteral("colour")))},
            {QStringLiteral("accentName"), accent.value(QStringLiteral("name")).toString()},
            {QStringLiteral("textOnAccent"), jsonColour(theme.value(QStringLiteral("textOnAccent")))},
            {QStringLiteral("glass"), jsonColour(theme.value(QStringLiteral("glass")))},
            {QStringLiteral("taskbarGlass"), jsonColour(theme.value(QStringLiteral("taskbarGlass")))},
            {QStringLiteral("wallpaper"), theme.value(QStringLiteral("wallpaper")).toString()},
            {QStringLiteral("moreWallpaperNames"), more},
            {QStringLiteral("underglow"), theme.value(QStringLiteral("underglow")).toBool()},
            {QStringLiteral("isDefault"), theme.value(QStringLiteral("default")).toBool()},
        };
    }
}

QVariantMap Themes::currentTheme() const
{
    for (const QVariant &theme : m_themes) {
        if (theme.toMap().value(QStringLiteral("id")).toString() == m_current) {
            return theme.toMap();
        }
    }
    return {};
}

void Themes::read()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();

    const QString current = config->group(QStringLiteral("Theme")).readEntry("Id", defaultTheme);
    if (current != m_current) {
        m_current = current;
        Q_EMIT currentChanged();
    }

    const KConfigGroup underglow = config->group(QStringLiteral("Underglow"));
    m_underglow = underglow.readEntry("Enabled", false);
    m_underglowFromTheme = underglow.readEntry("FromTheme", false);
    m_underglowColour = underglow.readEntry("Colour", QStringLiteral("accent"));
    if (m_underglowColour != QLatin1String("custom") && m_underglowColour != QLatin1String("rgb")) {
        m_underglowColour = QStringLiteral("accent");
    }
    m_underglowCustom = QColor(underglow.readEntry("Custom", defaultCustom.name()));
    if (!m_underglowCustom.isValid()) {
        m_underglowCustom = defaultCustom;
    }
    m_underglowEffect = underglow.readEntry("Effect", QString());
    if (m_underglowColour == QLatin1String("rgb")) {
        m_underglowEffect = QStringLiteral("flowing");
    } else if (m_underglowEffect != QLatin1String("breathing")) {
        m_underglowEffect = QStringLiteral("steady");
    }
    m_lightingRgb = config->group(QStringLiteral("Lighting")).readEntry("Rgb", false);
    Q_EMIT underglowChanged();
}

namespace
{
// wildos-auto-theme keeps Start and the flyouts' Plasma style in step with
// the theme. It starts with the session, so after an update the one running
// is the old program until the next sign-in, and a version from before
// themes never switches the style. A running copy whose program file has
// been replaced is swapped for the new one.
void restartStaleAutoTheme()
{
    bool stale = false;
    const QStringList pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool number = false;
        const int value = pid.toInt(&number);
        if (!number || QFileInfo(QStringLiteral("/proc/") + pid).ownerId() != getuid()) {
            continue;
        }
        const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
        if (exe.startsWith(QStringLiteral("/usr/bin/wildos-auto-theme")) && exe.endsWith(QStringLiteral(" (deleted)"))) {
            ::kill(value, SIGTERM);
            stale = true;
        }
    }
    if (stale) {
        QProcess::startDetached(QStringLiteral("wildos-auto-theme"), {});
    }
}
}

void Themes::apply(const QString &id)
{
    QVariantMap theme;
    for (const QVariant &entry : std::as_const(m_themes)) {
        if (entry.toMap().value(QStringLiteral("id")).toString() == id) {
            theme = entry.toMap();
        }
    }
    if (theme.isEmpty()) {
        return;
    }

    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    config->group(QStringLiteral("Theme")).writeEntry("Id", id, KConfig::Notify);

    // Underglow that came on with a theme goes off with it; underglow the
    // person turned on stays. A theme with underglow brings its own look.
    KConfigGroup underglow = config->group(QStringLiteral("Underglow"));
    if (theme.value(QStringLiteral("underglow")).toBool()) {
        const bool theirs = m_underglow && !m_underglowFromTheme;
        underglow.writeEntry("Enabled", true, KConfig::Notify);
        underglow.writeEntry("Colour", QStringLiteral("accent"), KConfig::Notify);
        underglow.writeEntry("Effect", QStringLiteral("steady"), KConfig::Notify);
        underglow.writeEntry("FromTheme", !theirs, KConfig::Notify);
    } else if (m_underglow && m_underglowFromTheme) {
        underglow.writeEntry("Enabled", false, KConfig::Notify);
        underglow.writeEntry("FromTheme", false, KConfig::Notify);
    }
    config->sync();
    read();
    restartStaleAutoTheme();

    QQmlEngine *engine = qmlEngine(this);
    auto system = engine ? engine->singletonInstance<System *>("WildOS.Settings", "System") : nullptr;
    if (!system) {
        return;
    }
    const QString scheme = theme.value(QStringLiteral("mode")).toString() == QLatin1String("light") ? QStringLiteral("WildOSLight") : QStringLiteral("WildOS");
    system->setColours(scheme, theme.value(QStringLiteral("accent")).value<QColor>());
    const QString wallpaper = theme.value(QStringLiteral("wallpaper")).toString();
    if (QFileInfo::exists(wallpaper)) {
        system->setWallpaper(wallpaper);
    }
}

// A change made on this page makes the underglow the person's own, so a
// later theme without underglow leaves it on.
void Themes::writeUnderglow(const QString &key, const QVariant &value)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    KConfigGroup group = config->group(QStringLiteral("Underglow"));
    group.writeEntry(key.toUtf8().constData(), value, KConfig::Notify);
    group.writeEntry("FromTheme", false, KConfig::Notify);
    config->sync();
    read();
}

void Themes::setUnderglow(bool on)
{
    writeUnderglow(QStringLiteral("Enabled"), on);
}

void Themes::setUnderglowColour(const QString &colour)
{
    if (colour != QLatin1String("accent") && colour != QLatin1String("custom") && colour != QLatin1String("rgb")) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    KConfigGroup group = config->group(QStringLiteral("Underglow"));
    group.writeEntry("Colour", colour, KConfig::Notify);
    // RGB only flows; a single colour is steady or breathing.
    if (colour == QLatin1String("rgb")) {
        group.writeEntry("Effect", QStringLiteral("flowing"), KConfig::Notify);
    } else if (m_underglowEffect == QLatin1String("flowing")) {
        group.writeEntry("Effect", QStringLiteral("steady"), KConfig::Notify);
    }
    if (colour == QLatin1String("custom") && !group.hasKey("Custom")) {
        group.writeEntry("Custom", m_underglowCustom.name(), KConfig::Notify);
    }
    group.writeEntry("FromTheme", false, KConfig::Notify);
    config->sync();
    read();
}

void Themes::setUnderglowCustom(const QColor &colour)
{
    if (!colour.isValid()) {
        return;
    }
    writeUnderglow(QStringLiteral("Custom"), colour.name(QColor::HexRgb));
}

void Themes::setUnderglowEffect(const QString &effect)
{
    const bool rgb = m_underglowColour == QLatin1String("rgb");
    if (rgb ? effect != QLatin1String("flowing") : effect != QLatin1String("steady") && effect != QLatin1String("breathing")) {
        return;
    }
    writeUnderglow(QStringLiteral("Effect"), effect);
}

void Themes::setLightingRgb(bool on)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    config->group(QStringLiteral("Lighting")).writeEntry("Rgb", on, KConfig::Notify);
    config->sync();
    read();
}

void Themes::refreshRgbDevices()
{
    if (m_rgbProbe) {
        return;
    }
    if (!QFileInfo(rgbTool).isExecutable()) {
        if (m_rgbDevices != 0) {
            m_rgbDevices = 0;
            Q_EMIT rgbDevicesChanged();
        }
        return;
    }
    m_rgbProbe = new QProcess(this);
    // Looking for lighting can mean waking USB devices; a probe that hangs
    // counts as none rather than holding the row back for ever.
    QTimer::singleShot(15000, m_rgbProbe, [probe = m_rgbProbe] {
        probe->kill();
    });
    connect(m_rgbProbe, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const int count = status == QProcess::NormalExit && code == 0 ? QString::fromUtf8(m_rgbProbe->readAllStandardOutput()).trimmed().toInt() : 0;
        m_rgbProbe->deleteLater();
        m_rgbProbe = nullptr;
        if (count != m_rgbDevices) {
            m_rgbDevices = qMax(0, count);
            Q_EMIT rgbDevicesChanged();
        }
    });
    connect(m_rgbProbe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_rgbProbe) {
            m_rgbProbe->deleteLater();
            m_rgbProbe = nullptr;
        }
    });
    m_rgbProbe->start(rgbTool, {});
}

QString Themes::accentName(const QColor &colour) const
{
    for (const QVariant &theme : m_themes) {
        const QVariantMap map = theme.toMap();
        if (map.value(QStringLiteral("accent")).value<QColor>().rgb() == colour.rgb()) {
            return map.value(QStringLiteral("accentName")).toString();
        }
    }
    return {};
}
