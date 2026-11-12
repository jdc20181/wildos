#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Where Voice keeps things. Recordings, their markers and their
// transcripts are plain files in the Recordings folder in the home folder
// (there is no XDG folder for them); the speech model is the app's own
// business and lives under ~/.local/share/wildos-voice.
namespace VoicePaths
{
inline QString recordingsFolder()
{
    return QDir::home().filePath(QStringLiteral("Recordings"));
}

inline QString dataFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("wildos-voice"));
}

inline QString modelFolder()
{
    return QDir(dataFolder()).filePath(QStringLiteral("models"));
}

// A recording's markers and its transcript sit beside it with the same
// name: "Recording 4.opus", "Recording 4.json", "Recording 4.txt".
inline QString sidecarOf(const QString &opusPath)
{
    return opusPath.left(opusPath.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".json");
}

inline QString transcriptOf(const QString &opusPath)
{
    return opusPath.left(opusPath.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".txt");
}
}
