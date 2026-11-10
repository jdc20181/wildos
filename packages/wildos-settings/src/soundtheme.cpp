#include "soundtheme.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QProcess>
#include <QStandardPaths>

namespace
{
KConfigGroup sounds()
{
    return KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->group(QStringLiteral("Sounds"));
}

KConfigGroup startEvent()
{
    return KSharedConfig::openConfig(QStringLiteral("plasma_workspace.notifyrc"))->group(QStringLiteral("Event/startkde"));
}
}

SoundTheme::SoundTheme(QObject *parent)
    : QObject(parent)
{
}

QString SoundTheme::scheme() const
{
    const auto g = sounds();
    if (!g.readEntry("Enable", true)) {
        return QStringLiteral("none");
    }
    return g.readEntry("Theme", QStringLiteral("ocean")) == QLatin1String("wildos") ? QStringLiteral("wildos") : QStringLiteral("ocean");
}

bool SoundTheme::startupSound() const
{
    return startEvent().readEntry("Action", QString()).contains(QLatin1String("Sound"));
}

void SoundTheme::setScheme(const QString &scheme)
{
    auto g = sounds();
    if (scheme == QLatin1String("none")) {
        g.writeEntry("Enable", false, KConfig::Notify);
    } else {
        // A true Enable is the default, so it isn't written down.
        g.revertToDefault("Enable", KConfig::Notify);
        g.writeEntry("Theme", scheme == QLatin1String("ocean") ? QStringLiteral("ocean") : QStringLiteral("wildos"), KConfig::Notify);
    }
    g.sync();
    Q_EMIT changed();
    if (scheme != QLatin1String("none")) {
        preview(QStringLiteral("message-new-instant"));
    }
}

void SoundTheme::setStartupSound(bool on)
{
    auto g = startEvent();
    g.writeEntry("Action", on ? QStringLiteral("Sound") : QString(), KConfig::Notify);
    g.sync();
    Q_EMIT changed();
}

void SoundTheme::preview(const QString &name)
{
    // The file from the scheme chosen here, played straight through
    // PipeWire: canberra-gtk-play would follow GTK's sound theme setting
    // rather than KDE's.
    const QString theme = sounds().readEntry("Theme", QStringLiteral("ocean"));
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                QStringLiteral("sounds/%1/stereo/%2.oga").arg(theme, name));
    if (file.isEmpty()) {
        return;
    }
    const QString player = !QStandardPaths::findExecutable(QStringLiteral("pw-play")).isEmpty() ? QStringLiteral("pw-play")
                                                                                                  : QStringLiteral("paplay");
    QProcess::startDetached(player, {file});
}
