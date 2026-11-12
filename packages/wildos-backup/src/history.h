#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

// The runs so far, newest first, in ~/.local/state/wildos-backup, so the
// Overview can list them with the drive unplugged. Each entry:
//   at          when the run started (ISO 8601)
//   finished    when it ended
//   result      "done", "failed" or "stopped"
//   reason      for a failed run, a sentence for the person
//   warning     done, but with something to say (files it couldn't read)
//   missedAt    a catch-up run: when the backup was due
//   missedWhy   "drive" (not plugged in then) or "late" (PC off or asleep)
//   dataAdded   bytes the drive took
//   filesNew, filesChanged, filesTotal, bytesTotal, seconds, snapshot
//   drive, driveName, kind ("scheduled", "manual" or "catch-up")
//   windows     "done", "running" (skipped), "failed" or absent
namespace History
{
QJsonArray load();
void append(const QJsonObject &entry);

// A run that found no drive leaves a note, so plugging the drive in later
// catches up (and the Overview can say a backup is waiting).
QDateTime pendingSince();
void setPending(const QDateTime &since);
void clearPending();
}
