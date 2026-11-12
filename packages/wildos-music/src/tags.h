#pragma once

#include "track.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// Reading songs with TagLib. Safe on any thread: nothing here touches Qt's
// GUI objects except QImage, which is fine off the GUI thread.
namespace Tags
{
// The file name endings Music treats as songs.
const QStringList &extensions();
bool isAudio(const QString &path);

// Tags, length and art. A file TagLib cannot read still comes back, named
// by its file, as long as its ending is a known one; the player finds its
// length as it plays.
Track read(const QFileInfo &file);
// The song's own lyrics, if its tags have any.
QString lyrics(const QString &path);

// Saves a picture to the cover folder (once per picture) and returns its
// file name there, or an empty string if it is not a picture.
QString storeCover(const QByteArray &data);
}
