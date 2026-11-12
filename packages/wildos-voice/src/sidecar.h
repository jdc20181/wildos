#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// What Voice knows about a recording beyond the sound: when it was made,
// how long it is, its markers and its title. Kept as a small JSON file
// beside the recording ("Recording 4.json"), so the folder stays plain
// files that anything can read, copy or back up.
struct Sidecar
{
    QString title;
    QDateTime created;
    // Times in milliseconds from the start of the recording.
    qint64 durationMs = -1;
    QList<qint64> markers;

    static Sidecar read(const QString &opusPath);
    // Written to a new file and renamed over the old one, so a crash
    // mid-write never leaves half a file.
    bool write(const QString &opusPath) const;
};
