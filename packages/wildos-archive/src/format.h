#pragma once

#include <QDateTime>
#include <QString>

namespace Format
{
// "48 KB", "4.8 MB", "1.2 GB", as the board writes them.
QString size(qint64 bytes);
// The regional short date and the clock chosen in Settings: 07/11/2026 19:02.
QString date(const QDateTime &when);
// "JPEG image", "Text", "Folder".
QString typeName(const QString &fileName, bool isDir);
bool isImage(const QString &fileName);
// "less than a second", "about 2 seconds", "about 3 minutes".
QString duration(double seconds);
// A folder as people know it: ~/Pictures/Trip is "Pictures › Trip".
QString folder(const QString &path);
// The archive's name without its endings: "Photos.tar.gz" is "Photos".
QString archiveBaseName(const QString &fileName);
// A name in `folder` that isn't taken yet: "Photos", "Photos (2)".
QString uniqueName(const QString &folder, const QString &base, const QString &suffix);
}
