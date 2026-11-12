#include "keepawake.h"

#include <KConfigGroup>
#include <KSharedConfig>

KeepAwake::KeepAwake(QObject *parent)
    : QObject(parent)
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc"))))
{
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("KeepAwake")) {
            read();
        }
    });
    read();
}

void KeepAwake::read()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const bool active = config->group(QStringLiteral("KeepAwake")).readEntry("Active", false);
    if (active != m_active) {
        m_active = active;
        Q_EMIT activeChanged();
    }
}

void KeepAwake::turnOff()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->group(QStringLiteral("KeepAwake")).writeEntry("Active", false, KConfig::Notify);
    config->sync();
    read();
}
