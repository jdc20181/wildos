#include "library.h"
#include "paths.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace
{
Library *s_instance = nullptr;

const QSet<QString> &videoSuffixes()
{
    static const QSet<QString> suffixes{
        QStringLiteral("mp4"), QStringLiteral("m4v"), QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("mov"),
        QStringLiteral("avi"), QStringLiteral("wmv"), QStringLiteral("asf"), QStringLiteral("flv"), QStringLiteral("f4v"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("m2v"), QStringLiteral("ts"), QStringLiteral("m2ts"),
        QStringLiteral("mts"), QStringLiteral("3gp"), QStringLiteral("3g2"), QStringLiteral("ogv"), QStringLiteral("ogm"),
        QStringLiteral("dv"), QStringLiteral("vob"), QStringLiteral("divx"), QStringLiteral("mxf"), QStringLiteral("mk3d"),
    };
    return suffixes;
}

QCollator &collator()
{
    // "Episode 2" before "Episode 10", as Files sorts.
    static QCollator c = [] {
        QCollator made;
        made.setNumericMode(true);
        made.setCaseSensitivity(Qt::CaseInsensitive);
        return made;
    }();
    return c;
}

// Whether a folder holds a video within a few levels, without walking a
// whole disk for it.
bool holdsVideos(const QString &folder, int depth, int *count, QString *cover)
{
    const QDir dir(folder);
    QStringList files = dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort);
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        return collator().compare(a, b) < 0;
    });
    for (const QString &name : std::as_const(files)) {
        if (Library::isVideo(name)) {
            if (cover->isEmpty()) {
                *cover = dir.filePath(name);
            }
            ++*count;
        }
    }
    if (depth > 0) {
        for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name)) {
            holdsVideos(dir.filePath(sub), depth - 1, count, cover);
        }
    }
    return *count > 0;
}
}

Library::Library(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Library::folderChanged);
}

Library *Library::instance()
{
    if (!s_instance) {
        new Library(QCoreApplication::instance());
    }
    return s_instance;
}

Library *Library::create(QQmlEngine *, QJSEngine *)
{
    Library *library = instance();
    QQmlEngine::setObjectOwnership(library, QQmlEngine::CppOwnership);
    return library;
}

QString Library::videosFolder() const
{
    return VideosPaths::videosFolder();
}

bool Library::isVideo(const QString &path)
{
    const qsizetype dot = path.lastIndexOf(QLatin1Char('.'));
    return dot > 0 && videoSuffixes().contains(path.mid(dot + 1).toLower());
}

QStringList Library::videosIn(const QString &folder) const
{
    if (folder.isEmpty()) {
        return {};
    }
    const QDir dir(folder);
    QStringList names;
    for (const QString &name : dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort)) {
        if (isVideo(name) && !name.startsWith(QLatin1Char('.'))) {
            names.append(name);
        }
    }
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return collator().compare(a, b) < 0;
    });
    QStringList paths;
    paths.reserve(names.size());
    for (const QString &name : std::as_const(names)) {
        paths.append(dir.filePath(name));
    }
    return paths;
}

QVariantList Library::foldersIn(const QString &folder) const
{
    const QDir dir(folder);
    QStringList subs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::NoSort);
    std::sort(subs.begin(), subs.end(), [](const QString &a, const QString &b) {
        return collator().compare(a, b) < 0;
    });
    QVariantList folders;
    for (const QString &sub : std::as_const(subs)) {
        const QString path = dir.filePath(sub);
        int count = 0;
        QString cover;
        if (holdsVideos(path, 2, &count, &cover)) {
            folders.append(QVariantMap{
                {QStringLiteral("path"), path},
                {QStringLiteral("name"), sub},
                {QStringLiteral("count"), count},
                {QStringLiteral("cover"), cover},
            });
        }
    }
    return folders;
}

QString Library::nameOf(const QString &path) const
{
    return QFileInfo(path).completeBaseName();
}

QString Library::parentOf(const QString &path) const
{
    return QFileInfo(path).absolutePath();
}

bool Library::exists(const QString &path) const
{
    return QFileInfo::exists(path);
}

QString Library::placeOf(const QString &folder) const
{
    if (folder.isEmpty()) {
        return {};
    }
    const QString clean = QDir::cleanPath(folder);
    const QString videos = QDir::cleanPath(videosFolder());
    const QString home = QDir::cleanPath(QDir::homePath());
    const QString arrow = QStringLiteral(" › ");
    QStringList parts;
    if (clean == videos || clean.startsWith(videos + QLatin1Char('/'))) {
        parts.append(QCoreApplication::translate("Library", "Videos"));
        parts.append(clean.mid(videos.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts));
    } else if (clean == home) {
        parts.append(QCoreApplication::translate("Library", "Home"));
    } else if (clean.startsWith(home + QLatin1Char('/'))) {
        parts = clean.mid(home.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    } else {
        parts.append(QFileInfo(clean).fileName().isEmpty() ? clean : QFileInfo(clean).fileName());
    }
    // The last two places say enough; a deep path would cover the picture.
    if (parts.size() > 2) {
        parts = parts.mid(parts.size() - 2);
    }
    return parts.join(arrow);
}

void Library::showInFiles(const QString &path) const
{
    const QUrl url = QUrl::fromLocalFile(path);
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                                                          QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    message << QStringList{url.toString()} << QString();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }
}

QString Library::pathFromUrl(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

void Library::watch(const QString &folder)
{
    const QStringList watched = m_watcher.directories();
    if (watched.size() == 1 && watched.first() == folder) {
        return;
    }
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    if (!folder.isEmpty() && QFileInfo(folder).isDir()) {
        m_watcher.addPath(folder);
    }
}
