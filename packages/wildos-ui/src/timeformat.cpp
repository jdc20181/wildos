#include "timeformat.h"

#include "clockformat.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QLocale>

TimeFormat::TimeFormat(QObject *parent)
    : QObject(parent)
{
    // The sign-in screen runs as the sddm user, who has no wildosrc, so it
    // keeps the regional format's own clock.
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc")));
    if (m_watcher) {
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() == QLatin1String("Clock")) {
                read();
            }
        });
    }
    read();
}

void TimeFormat::read()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("wildosrc"));
    config->reparseConfiguration();
    const int value = config->group(QStringLiteral("Clock")).readEntry("Hours", 0);
    const int hours = value == 12 || value == 24 ? value : 0;
    if (hours != m_hours) {
        m_hours = hours;
        Q_EMIT changed();
    }
}

QString TimeFormat::shortFormat() const
{
    return ClockFormat::rewrite(QLocale::system().timeFormat(QLocale::ShortFormat), m_hours);
}

QString TimeFormat::secondsFormat() const
{
    return ClockFormat::withSeconds(shortFormat());
}

QString TimeFormat::withoutMarker(const QDateTime &when) const
{
    return ClockFormat::withoutMarker(QLocale::system(), when.toLocalTime().time(), shortFormat());
}
