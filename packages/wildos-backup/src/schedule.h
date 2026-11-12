#pragma once

#include <QString>
#include <QStringList>

class Settings;

// The systemd user units that run backups. They are installed for everyone
// in /usr/lib/systemd/user but only switched on for a person once they set
// a backup up:
//
//   wildos-backup.timer          the schedule (09:00 every day, Persistent,
//                                so a time missed while the PC was off runs
//                                at the next start); the app overrides the
//                                time and days with a drop-in
//   wildos-backup@.service       one run: @scheduled, @manual or @catch-up
//   wildos-backup-watch.service  waits for a backup drive to be plugged in
//                                and starts @catch-up if a run was missed
namespace Schedule
{
// The timer's OnCalendar for these days and minutes after midnight.
QString onCalendar(const QList<int> &days, int minutes);
// Writes the drop-in and switches the units on or off to match.
void apply(const Settings *settings);
// Starts a run in the background; it outlives the window.
void startRun(const QString &kind);
}
