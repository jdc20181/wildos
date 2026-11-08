#include "taskbarstyle.h"

#include <KConfigGroup>
#include <KSharedConfig>

TaskbarStyle::TaskbarStyle(QObject *parent)
    : QObject(parent)
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc"))))
{
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Taskbar")) {
            read();
        }
    });
    read();
}

void TaskbarStyle::read()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const bool floating = config->group(QStringLiteral("Taskbar")).readEntry("Style") == QLatin1String("floating");
    if (floating != m_floating) {
        m_floating = floating;
        Q_EMIT floatingChanged();
    }
}
