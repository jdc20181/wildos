#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Where Videos keeps things. The videos stay where they are; the places
// you stopped in them live under ~/.local/share/wildos-videos, and the
// thumbnails, which can always be made again, under ~/.cache/wildos-videos.
namespace VideosPaths
{
inline QString videosFolder()
{
    const QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return videos.isEmpty() ? QDir::home().filePath(QStringLiteral("Videos")) : videos;
}

inline QString dataFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("wildos-videos"));
}

inline QString positionsFile()
{
    return QDir(dataFolder()).filePath(QStringLiteral("positions.json"));
}

inline QString thumbnailFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)).filePath(QStringLiteral("wildos-videos"));
}
}
