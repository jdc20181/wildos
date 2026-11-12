#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Where Music keeps things. The songs and playlists stay in the Music
// folder, readable by anything; the app's own index, covers and state are
// its business and live under ~/.local/share/wildos-music.
namespace MusicPaths
{
inline QString musicFolder()
{
    const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return music.isEmpty() ? QDir::home().filePath(QStringLiteral("Music")) : music;
}

inline QString playlistFolder()
{
    return QDir(musicFolder()).filePath(QStringLiteral("Playlists"));
}

inline QString dataFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("wildos-music"));
}

inline QString indexFile()
{
    return QDir(dataFolder()).filePath(QStringLiteral("library.json"));
}

inline QString stateFile()
{
    return QDir(dataFolder()).filePath(QStringLiteral("state.json"));
}

// Covers are cut down copies of the art in the files, named by a hash of
// the picture, so an album's eleven songs share one file.
inline QString coverFolder()
{
    return QDir(dataFolder()).filePath(QStringLiteral("covers"));
}
}
