#pragma once

#include "track.h"

#include <QStringList>

#include <atomic>
#include <functional>

// Walks the Music folder. Songs whose file has not changed since the last
// walk keep what the index says; only new and changed files are read, so
// a walk over an unchanged library only looks at file dates and sizes.
namespace Scanner
{
struct Result
{
    TrackList tracks;
    // Every folder under the root, for the watcher.
    QStringList folders;
    bool changed = false;
};

// progress receives the songs found so far now and then while new files
// are read, so a first walk over a big library fills the window as it goes.
Result scan(const QString &root, const TrackList &previous, const std::atomic_bool &stop,
            const std::function<void(const TrackList &)> &progress);
}
