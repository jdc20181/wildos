#include "olderapps.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDBusConnection>
#include <QDBusMessage>

namespace
{
const QStringList choices = {QStringLiteral("None"), QStringLiteral("Modifiers"), QStringLiteral("Combinations"), QStringLiteral("All")};

KConfigGroup xwayland(KConfig &config)
{
    return config.group(QStringLiteral("Xwayland"));
}
}

OlderApps::OlderApps(QObject *parent)
    : QObject(parent)
{
}

int OlderApps::keys() const
{
    KConfig config(QStringLiteral("kwinrc"));
    const QString value = xwayland(config).readEntry("XwaylandEavesdrops", QStringLiteral("Combinations"));
    // KConfigSkeleton matches the name without case and falls back to the
    // number, so both are read the same way here.
    for (int i = 0; i < choices.size(); ++i) {
        if (value.compare(choices.at(i), Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    bool ok = false;
    const int number = value.toInt(&ok);
    return ok && number >= 0 && number < choices.size() ? number : 2;
}

void OlderApps::setKeys(int level)
{
    if (level < 0 || level >= choices.size()) {
        return;
    }
    KConfig config(QStringLiteral("kwinrc"));
    xwayland(config).writeEntry("XwaylandEavesdrops", choices.at(level), KConfig::Notify);
    config.sync();
    reconfigure();
}

bool OlderApps::mouse() const
{
    KConfig config(QStringLiteral("kwinrc"));
    return xwayland(config).readEntry("XwaylandEavesdropsMouse", false);
}

void OlderApps::setMouse(bool on)
{
    KConfig config(QStringLiteral("kwinrc"));
    xwayland(config).writeEntry("XwaylandEavesdropsMouse", on, KConfig::Notify);
    config.sync();
    reconfigure();
}

void OlderApps::reconfigure()
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"),
                                                  QStringLiteral("reconfigure"));
    QDBusConnection::sessionBus().send(message);
    Q_EMIT changed();
}
