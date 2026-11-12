#include "scanner.h"
#include "tags.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>

namespace
{
// A cover.jpg or folder.jpg beside the songs stands in for art the files
// do not carry, as most players read it.
QString folderCover(const QString &folder, QHash<QString, QString> &seen)
{
    const auto known = seen.constFind(folder);
    if (known != seen.cend()) {
        return *known;
    }
    static const QStringList names{
        QStringLiteral("cover"), QStringLiteral("folder"), QStringLiteral("front"), QStringLiteral("album"), QStringLiteral("albumart"),
    };
    QString found;
    const QFileInfoList pictures = QDir(folder).entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp")},
                                                             QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QString &name : names) {
        for (const QFileInfo &picture : pictures) {
            if (picture.completeBaseName().compare(name, Qt::CaseInsensitive) == 0) {
                QFile file(picture.absoluteFilePath());
                if (file.size() < 20 * 1024 * 1024 && file.open(QIODevice::ReadOnly)) {
                    found = Tags::storeCover(file.readAll());
                }
                break;
            }
        }
        if (!found.isEmpty()) {
            break;
        }
    }
    seen.insert(folder, found);
    return found;
}
}

Scanner::Result Scanner::scan(const QString &root, const TrackList &previous, const std::atomic_bool &stop,
                              const std::function<void(const TrackList &)> &progress)
{
    Result result;
    QHash<QString, const Track *> before;
    before.reserve(previous.size());
    for (const Track &t : previous) {
        before.insert(t.path, &t);
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QHash<QString, QString> folderCovers;
    QElapsedTimer sinceProgress;
    sinceProgress.start();

    result.folders.append(QDir(root).absolutePath());
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (stop) {
            result.changed = false;
            return result;
        }
        const QFileInfo info = it.nextFileInfo();
        if (info.isDir()) {
            result.folders.append(info.absoluteFilePath());
            continue;
        }
        if (!Tags::isAudio(info.fileName())) {
            continue;
        }
        const QString path = info.absoluteFilePath();
        const Track *old = before.value(path);
        if (old && old->modified == info.lastModified().toSecsSinceEpoch() && old->size == info.size()) {
            result.tracks.append(*old);
            before.remove(path);
            continue;
        }
        Track t = Tags::read(info);
        t.added = old ? old->added : now;
        if (t.cover.isEmpty()) {
            t.cover = folderCover(info.absolutePath(), folderCovers);
        }
        before.remove(path);
        result.tracks.append(t);
        result.changed = true;
        // Only on a first walk: later walks would show a library with the
        // songs not reached yet missing.
        if (progress && previous.isEmpty() && sinceProgress.elapsed() > 700) {
            progress(result.tracks);
            sinceProgress.restart();
        }
    }
    // Anything left in `before` has gone from the folder.
    if (!before.isEmpty()) {
        result.changed = true;
    }
    return result;
}
