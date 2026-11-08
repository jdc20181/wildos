#include "setup.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KUser>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace
{
const QString defaultWallpaper = QStringLiteral("/usr/share/wallpapers/WildOS");
const QString flagFile = QStringLiteral("wildos-welcomerc");

QString localPath(const QString &urlOrPath)
{
    QString path = urlOrPath.startsWith(QLatin1String("file:")) ? QUrl(urlOrPath).toLocalFile() : urlOrPath;
    while (path.length() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

// A wallpaper package holds the same photo at many sizes; the sheet covers
// the whole screen, so it wants the largest.
QString largestImage(const QString &package)
{
    QDir images(package + QStringLiteral("/contents/images"));
    QString best;
    qint64 bestPixels = 0;
    const auto entries = images.entryList(QDir::Files);
    for (const QString &name : entries) {
        const auto match = QRegularExpression(QStringLiteral("^(\\d+)x(\\d+)")).match(name);
        const qint64 pixels = match.hasMatch() ? match.captured(1).toLongLong() * match.captured(2).toLongLong() : 1;
        if (pixels > bestPixels) {
            bestPixels = pixels;
            best = images.filePath(name);
        }
    }
    return best;
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool processRunning(const QString &name)
{
    const auto pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        if (pid.at(0).isDigit() && readFile(QStringLiteral("/proc/%1/comm").arg(pid)).trimmed() == name) {
            return true;
        }
    }
    return false;
}

// Finds the spacer that sits just before Start on the bar. The layout
// script puts an expanding spacer either side of Start and the apps, which
// is what centres them; making the left one collapse puts them on the left
// like Windows 10.
const QString findSpacer = QStringLiteral(
    "var bar = panels().length ? panels()[0] : null;"
    "var spacer = null;"
    "var foundStart = false;"
    "if (bar) {"
    "  var ws = bar.widgets();"
    "  var byId = {};"
    "  for (var i = 0; i < ws.length; i++) byId[ws[i].id] = ws[i];"
    "  bar.currentConfigGroup = ['General'];"
    "  var order = String(bar.readConfig('AppletOrder', '')).split(';');"
    "  var ordered = [];"
    "  for (var j = 0; j < order.length; j++) if (byId[order[j]]) ordered.push(byId[order[j]]);"
    "  if (ordered.length !== ws.length) ordered = ws;"
    "  var candidate = null;"
    "  for (var k = 0; k < ordered.length; k++) {"
    "    if (ordered[k].type === 'org.wildos.start') { spacer = candidate; foundStart = true; break; }"
    "    if (ordered[k].type === 'org.kde.plasma.panelspacer') candidate = ordered[k];"
    "  }"
    "}");
}

static bool s_planning = false;

void Setup::setPlanning(bool planning)
{
    s_planning = planning;
}

bool Setup::planning()
{
    return s_planning;
}

Setup::Setup(QObject *parent)
    : QObject(parent)
{
    readAccent();
    readColourScheme();
    readWallpaper();
    readTaskbarLayout();
}

QString Setup::firstName() const
{
    const KUser user;
    const QString full = user.property(KUser::FullName).toString().section(QLatin1Char(','), 0, 0).trimmed();
    const QString first = full.section(QLatin1Char(' '), 0, 0);
    return first.isEmpty() ? user.loginName() : first;
}

void Setup::readWallpaper()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"));
    KConfigGroup containments(config, QStringLiteral("Containments"));
    QString found;
    const auto ids = containments.groupList();
    for (const QString &id : ids) {
        const QString image = containments.group(id)
                                  .group(QStringLiteral("Wallpaper"))
                                  .group(QStringLiteral("org.kde.image"))
                                  .group(QStringLiteral("General"))
                                  .readEntry("Image");
        if (!image.isEmpty()) {
            found = localPath(image);
            break;
        }
    }
    if (found.isEmpty()) {
        found = defaultWallpaper;
    }
    if (QFileInfo(found).isDir()) {
        found = largestImage(found);
    }
    m_wallpaper = found.isEmpty() ? QString() : QUrl::fromLocalFile(found).toString();
}

void Setup::readAccent()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    const QString value = config->group(QStringLiteral("General")).readEntry("AccentColor");
    QColor colour;
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() >= 3) {
        colour = QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
    } else if (!value.isEmpty()) {
        colour = QColor(value);
    }
    m_accent = colour.isValid() ? colour : QColor(QStringLiteral("#8fb4ff"));
}

void Setup::setAccent(const QColor &colour)
{
    // The same two steps as Settings > Personalisation > Colours: the tool
    // recolours selections and title bars and tells running apps, then the
    // key Wild OS widgets read is written after it so the tool's own write
    // of kdeglobals cannot undo it.
    const auto writeKey = [colour] {
        KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        config->reparseConfiguration();
        KConfigGroup general = config->group(QStringLiteral("General"));
        const QString rgb = QStringLiteral("%1,%2,%3").arg(colour.red()).arg(colour.green()).arg(colour.blue());
        general.writeEntry("AccentColor", rgb, KConfig::Notify);
        general.writeEntry("LastUsedCustomAccentColor", rgb, KConfig::Notify);
        config->sync();
    };

    if (hasProgram(QStringLiteral("plasma-apply-colorscheme"))) {
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [process, writeKey] {
            process->deleteLater();
            writeKey();
        });
        process->start(QStringLiteral("plasma-apply-colorscheme"), {QStringLiteral("--accent-color"), colour.name(QColor::HexRgb)});
    } else {
        writeKey();
    }
    m_accent = colour;
    Q_EMIT accentChanged();
}

void Setup::readColourScheme()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    m_colourScheme = config->group(QStringLiteral("General")).readEntry("ColorScheme", QStringLiteral("WildOS"));
    if (!s_planning) {
        KSharedConfigPtr wildos = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
        wildos->reparseConfiguration();
        m_automaticTheme = wildos->group(QStringLiteral("Theme")).readEntry("Mode") == QLatin1String("automatic");
    }
}

// The key Settings writes and wildos-auto-theme watches: while it says
// "automatic", that helper switches the theme at sunrise and sunset.
void Setup::writeThemeMode(const QString &mode)
{
    KSharedConfigPtr wildos = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    wildos->group(QStringLiteral("Theme")).writeEntry("Mode", mode, KConfig::Notify);
    wildos->sync();
}

// While planning, the choice only goes into the plan: the helper runs in the
// new account's own session, where apply() writes the key.
void Setup::setAutomaticTheme()
{
    if (!hasColourScheme(QStringLiteral("WildOSLight"))) {
        return;
    }
    m_automaticTheme = true;
    if (!s_planning) {
        writeThemeMode(QStringLiteral("automatic"));
    }
    Q_EMIT colourSchemeChanged();
}

bool Setup::hasColourScheme(const QString &name) const
{
    return !name.isEmpty()
        && !QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(name)).isEmpty();
}

// As Settings > Personalisation does it: the tool switches every app and
// writes ColorScheme, which WildOS.UI watches; applying a scheme resets the
// accent key, so the accent is put back once the tool is done.
void Setup::setColourScheme(const QString &name)
{
    if (!hasColourScheme(name)) {
        return;
    }
    if (m_automaticTheme || !s_planning) {
        m_automaticTheme = false;
        if (!s_planning) {
            writeThemeMode(name == QLatin1String("WildOSLight") ? QStringLiteral("light") : QStringLiteral("dark"));
        }
        Q_EMIT colourSchemeChanged();
    }
    if (name == m_colourScheme) {
        return;
    }
    const QColor accent = m_accent;
    if (hasProgram(QStringLiteral("plasma-apply-colorscheme"))) {
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, accent] {
            process->deleteLater();
            setAccent(accent);
        });
        process->start(QStringLiteral("plasma-apply-colorscheme"), {name});
    } else {
        KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        config->group(QStringLiteral("General")).writeEntry("ColorScheme", name, KConfig::Notify);
        config->sync();
    }
    m_colourScheme = name;
    Q_EMIT colourSchemeChanged();
}

QString Setup::evaluatePlasmaScript(const QString &script, bool *ok)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                                  QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("evaluateScript"));
    message << script;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 3000);
    const bool good = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty();
    if (ok) {
        *ok = good;
    }
    return good ? reply.arguments().constFirst().toString().trimmed() : QString();
}

void Setup::readTaskbarLayout()
{
    if (s_planning) {
        // Every new account starts with the bar centred.
        m_taskbarLayoutKnown = true;
        m_taskbarLayout = Centred;
        return;
    }
    bool ok = false;
    const QString answer = evaluatePlasmaScript(findSpacer
                                                    + QStringLiteral("if (spacer) { spacer.currentConfigGroup = ['General'];"
                                                                     " print(String(spacer.readConfig('expanding', true))); }"
                                                                     " else if (foundStart) { print('left'); }"),
                                                &ok);
    // Without the spacer the bar is already on the left and there is nothing
    // that would centre it again, so the choice is only offered with one.
    m_taskbarLayoutKnown = ok && (answer == QLatin1String("true") || answer == QLatin1String("false"));
    m_taskbarLayout = (answer == QLatin1String("false") || answer == QLatin1String("left")) ? LeftAligned : Centred;
    Q_EMIT taskbarLayoutChanged();
}

void Setup::setTaskbarLayout(int layout)
{
    const bool left = layout == LeftAligned;
    if (s_planning) {
        m_taskbarLayout = left ? LeftAligned : Centred;
        Q_EMIT taskbarLayoutChanged();
        return;
    }
    bool ok = false;
    const QString answer = evaluatePlasmaScript(findSpacer
                                                    + QStringLiteral("if (spacer) { spacer.currentConfigGroup = ['General'];"
                                                                     " spacer.writeConfig('expanding', %1);"
                                                                     " spacer.writeConfig('length', %2);"
                                                                     " spacer.reloadConfig(); print('ok'); }")
                                                          .arg(left ? QStringLiteral("false") : QStringLiteral("true"), left ? QStringLiteral("0") : QStringLiteral("32")),
                                                &ok);
    // The card only moves when the bar did; otherwise the choice is shown
    // as unavailable rather than pretending it worked.
    if (ok && answer == QLatin1String("ok")) {
        m_taskbarLayout = left ? LeftAligned : Centred;
    } else {
        m_taskbarLayoutKnown = false;
    }
    Q_EMIT taskbarLayoutChanged();
}

void Setup::finish()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(flagFile);
    KConfigGroup general = config->group(QStringLiteral("General"));
    general.writeEntry("Done", true);
    // Read by the autostart entry's start condition, so the session does not
    // even start this program again.
    general.writeEntry("ShowWelcome", false);
    config->sync();
}

void Setup::quit()
{
    QCoreApplication::quit();
}

bool Setup::alreadyDone()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(flagFile);
    return config->group(QStringLiteral("General")).readEntry("Done", false);
}

QString Setup::reasonNotToRun()
{
    // wildos-live creates this account for the live ISO; nobody keeps
    // anything there, so there is nothing to set up.
    if (KUser().loginName() == QLatin1String("live")) {
        return QStringLiteral("live user");
    }
    const QString cmdline = readFile(QStringLiteral("/proc/cmdline"));
    if (cmdline.contains(QLatin1String("rd.live.image")) || cmdline.contains(QLatin1String("boot=live"))
        || QFileInfo::exists(QStringLiteral("/run/initramfs/live"))) {
        return QStringLiteral("live session");
    }
    if (processRunning(QStringLiteral("calamares"))) {
        return QStringLiteral("installer running");
    }
    return {};
}

bool Setup::hasProgram(const QString &program)
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

QVariantMap Setup::plan() const
{
    return {
        {QStringLiteral("accent"), m_accent.name(QColor::HexRgb)},
        {QStringLiteral("colourScheme"), m_colourScheme},
        {QStringLiteral("automaticTheme"), m_automaticTheme},
        {QStringLiteral("taskbarLeft"), m_taskbarLayout == LeftAligned},
    };
}

void Setup::apply(const QVariantMap &plan, std::function<void()> done)
{
    const QColor accent(plan.value(QStringLiteral("accent")).toString());
    const QString scheme = plan.value(QStringLiteral("colourScheme")).toString();
    if (hasColourScheme(scheme) && scheme != m_colourScheme) {
        // Applying a scheme puts the accent back once it is done.
        if (accent.isValid()) {
            m_accent = accent;
        }
        setColourScheme(scheme);
    } else if (accent.isValid() && accent != m_accent) {
        setAccent(accent);
    }
    if (plan.value(QStringLiteral("automaticTheme")).toBool()) {
        setAutomaticTheme();
    }

    if (!plan.value(QStringLiteral("taskbarLeft")).toBool()) {
        done();
        return;
    }
    // The taskbar belongs to plasmashell, which may still be starting when
    // the welcome does at sign-in. It is asked again for up to a minute.
    auto *tries = new int(0);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer, tries, done] {
        readTaskbarLayout();
        if (m_taskbarLayoutKnown || ++*tries >= 30) {
            timer->stop();
            timer->deleteLater();
            delete tries;
            if (m_taskbarLayoutKnown) {
                setTaskbarLayout(LeftAligned);
            }
            done();
        }
    });
    timer->start(2000);
}
