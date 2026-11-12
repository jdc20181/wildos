#include "watcher.h"
#include "drives.h"
#include "schedule.h"
#include "settings.h"

Watcher::Watcher(QObject *parent)
    : QObject(parent)
{
    Drives *drives = Drives::instance();
    connect(drives, &Drives::appeared, this, [](const QString &uuid) {
        Settings *settings = Settings::instance();
        // The drive list may have changed in the app since this started.
        settings->reload();
        if (!settings->drive(uuid).isEmpty()) {
            Schedule::startRun(QStringLiteral("catch-up"));
        }
    });
    drives->watch();
}
