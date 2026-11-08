#pragma once

#include <QJsonArray>
#include <QList>

namespace Preload
{
// What these processes read from disk to start: every file they have
// mapped (the program, its libraries, plugins, fonts and data files), and
// of each only the parts now in memory, in 256 KiB chunks. Big libraries
// are mapped whole but only partly used, so this keeps preloading to what
// a start really touches.
QJsonArray sample(const QList<int> &pids);

// "wildos-launchd --preload": a while after login, reads the most used
// apps' files into the page cache at idle priority, within a memory budget,
// so their first start of the session is as quick as a second one.
int run();
}
