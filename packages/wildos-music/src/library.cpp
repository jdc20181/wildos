#include "library.h"
#include "paths.h"
#include "scanner.h"
#include "tags.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <climits>
#include <numeric>

namespace
{
Library *s_instance = nullptr;
constexpr int indexVersion = 1;

QCollator &collator()
{
    static QCollator c = [] {
        QCollator made;
        made.setNumericMode(true);
        made.setCaseSensitivity(Qt::CaseInsensitive);
        return made;
    }();
    return c;
}

// "The Beatles" files under B, as record shops and other players do.
QString sortName(const QString &name)
{
    if (name.startsWith(QLatin1String("The "), Qt::CaseInsensitive) && name.size() > 4) {
        return name.mid(4);
    }
    return name;
}

QString unknownArtist()
{
    return QCoreApplication::translate("Library", "Unknown artist");
}
}

Library::Library(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    qRegisterMetaType<TrackList>();
    QDir().mkpath(musicFolder());
    loadIndex();

    m_rescan.setSingleShot(true);
    m_rescan.setInterval(1500);
    connect(&m_rescan, &QTimer::timeout, this, &Library::startScan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        // Music writes its own playlists there; they hold no songs.
        if (QDir(path) == QDir(MusicPaths::playlistFolder())) {
            return;
        }
        m_rescan.start();
    });
    QTimer::singleShot(0, this, &Library::startScan);
}

Library::~Library()
{
    m_stop = true;
    if (m_thread) {
        m_thread->wait();
    }
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

QString Library::musicFolder() const
{
    return MusicPaths::musicFolder();
}

void Library::loadIndex()
{
    QFile file(MusicPaths::indexFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QLatin1String("version")).toInt() != indexVersion) {
        return;
    }
    const QJsonArray list = root.value(QLatin1String("tracks")).toArray();
    TrackList tracks;
    tracks.reserve(list.size());
    for (const QJsonValue &v : list) {
        Track t = Track::fromJson(v.toObject());
        if (!t.path.isEmpty()) {
            tracks.append(t);
        }
    }
    setTracks(tracks);
    m_ready = true;
}

void Library::saveIndex() const
{
    QJsonArray list;
    for (const Track &t : m_tracks) {
        list.append(t.toJson());
    }
    QDir().mkpath(MusicPaths::dataFolder());
    QSaveFile file(MusicPaths::indexFile());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), indexVersion}, {QStringLiteral("tracks"), list}}).toJson(QJsonDocument::Compact));
    file.commit();
}

void Library::setTracks(const TrackList &tracks)
{
    m_tracks = tracks;
    rebuild();
    ++m_revision;
    Q_EMIT changed();
}

void Library::rebuild()
{
    m_byPath.clear();
    m_albums.clear();
    m_artists.clear();
    m_byPath.reserve(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &t = m_tracks.at(i);
        m_byPath.insert(t.path, i);

        const QString artistName = t.filedArtist().isEmpty() ? unknownArtist() : t.filedArtist();
        Artist &artist = m_artists[artistName.toLower()];
        if (artist.name.isEmpty()) {
            artist.name = artistName;
        }
        artist.tracks.append(i);
        if (artist.cover.isEmpty()) {
            artist.cover = t.cover;
        }

        const QString key = t.albumKey();
        if (key.isEmpty()) {
            continue;
        }
        Album &album = m_albums[key];
        if (album.key.isEmpty()) {
            album.key = key;
            album.title = t.album;
            album.artist = artistName;
            if (!artist.albums.contains(key)) {
                artist.albums.append(key);
            }
        }
        album.tracks.append(i);
        album.duration += t.duration;
        album.year = std::max(album.year, t.year);
        album.added = std::max(album.added, t.added);
        if (album.cover.isEmpty()) {
            album.cover = t.cover;
        }
    }
    for (Album &album : m_albums) {
        album.tracks = sortedRows(album.tracks, QStringLiteral("number"));
    }
}

void Library::startScan()
{
    if (m_scanning) {
        m_scanAgain = true;
        return;
    }
    m_scanning = true;
    m_scanAgain = false;
    Q_EMIT scanningChanged();

    const QString root = musicFolder();
    const TrackList previous = m_tracks;
    m_thread = QThread::create([this, root, previous] {
        const auto progress = [this](const TrackList &sofar) {
            QMetaObject::invokeMethod(this, [this, sofar] {
                setTracks(sofar);
                if (!m_ready) {
                    m_ready = true;
                    Q_EMIT readyChanged();
                }
            }, Qt::QueuedConnection);
        };
        const Scanner::Result result = Scanner::scan(root, previous, m_stop, progress);
        if (m_stop) {
            return;
        }
        QMetaObject::invokeMethod(this, [this, result] {
            scanFinished(result.tracks, result.folders, result.changed);
        }, Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::destroyed, this, [this] {
        m_thread = nullptr;
    });
    m_thread->start(QThread::LowPriority);
}

void Library::scanFinished(const TrackList &tracks, const QStringList &folders, bool changed)
{
    if (changed) {
        setTracks(tracks);
        saveIndex();
    }
    watch(folders);
    m_scanning = false;
    Q_EMIT scanningChanged();
    if (!m_ready) {
        m_ready = true;
        Q_EMIT readyChanged();
    }
    if (m_scanAgain) {
        m_rescan.start();
    }
}

void Library::watch(const QStringList &folders)
{
    const QSet<QString> wanted(folders.cbegin(), folders.cend());
    const QStringList watched = m_watcher.directories();
    QStringList gone;
    for (const QString &dir : watched) {
        if (!wanted.contains(dir)) {
            gone.append(dir);
        }
    }
    if (!gone.isEmpty()) {
        m_watcher.removePaths(gone);
    }
    const QSet<QString> have(watched.cbegin(), watched.cend());
    QStringList added;
    for (const QString &dir : folders) {
        if (!have.contains(dir)) {
            added.append(dir);
        }
    }
    if (!added.isEmpty()) {
        m_watcher.addPaths(added);
    }
}

void Library::rescan()
{
    m_rescan.start(0);
}

const Track *Library::find(const QString &path)
{
    const auto row = m_byPath.constFind(path);
    if (row != m_byPath.cend()) {
        return &m_tracks.at(*row);
    }
    auto loose = m_loose.find(path);
    if (loose != m_loose.end()) {
        return &*loose;
    }
    const QFileInfo info(path);
    if (!info.isFile() || !Tags::isAudio(path)) {
        return nullptr;
    }
    return &*m_loose.insert(path, Tags::read(info));
}

QString Library::coverUrl(const Track &t) const
{
    if (t.cover.isEmpty()) {
        return QString();
    }
    return QUrl::fromLocalFile(QDir(MusicPaths::coverFolder()).filePath(t.cover)).toString();
}

QVariantMap Library::map(const Track &t) const
{
    return {
        {QStringLiteral("path"), t.path},
        {QStringLiteral("title"), t.title},
        {QStringLiteral("artist"), t.artist.isEmpty() ? unknownArtist() : t.artist},
        {QStringLiteral("album"), t.album},
        {QStringLiteral("albumKey"), t.albumKey()},
        {QStringLiteral("albumArtist"), t.filedArtist()},
        {QStringLiteral("year"), t.year},
        {QStringLiteral("number"), t.number},
        {QStringLiteral("disc"), t.disc},
        {QStringLiteral("duration"), t.duration},
        {QStringLiteral("cover"), coverUrl(t)},
        {QStringLiteral("lyrics"), t.lyrics},
    };
}

QVariantMap Library::track(const QString &path)
{
    const Track *t = find(path);
    return t ? map(*t) : QVariantMap();
}

QVariantList Library::tracks(const QStringList &paths)
{
    QVariantList list;
    list.reserve(paths.size());
    for (const QString &path : paths) {
        if (const Track *t = find(path)) {
            list.append(map(*t));
        } else {
            // Kept in its place, so rows still line up with the playlist
            // or queue it came from and can be taken out.
            Track gone;
            gone.path = path;
            gone.title = QFileInfo(path).completeBaseName();
            QVariantMap m = map(gone);
            m.insert(QStringLiteral("artist"), tr("Missing from the Music folder"));
            m.insert(QStringLiteral("missing"), true);
            list.append(m);
        }
    }
    return list;
}

QList<int> Library::sortedRows(QList<int> rows, const QString &by) const
{
    QCollator &c = collator();
    auto byTitle = [&](int a, int b) {
        return c.compare(m_tracks.at(a).title, m_tracks.at(b).title) < 0;
    };
    if (by == QLatin1String("number")) {
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            const Track &x = m_tracks.at(a);
            const Track &y = m_tracks.at(b);
            if (x.disc != y.disc) {
                return x.disc < y.disc;
            }
            if (x.number != y.number) {
                // Untagged songs after the numbered ones.
                return (x.number ? x.number : INT_MAX) < (y.number ? y.number : INT_MAX);
            }
            return c.compare(QFileInfo(x.path).fileName(), QFileInfo(y.path).fileName()) < 0;
        });
    } else if (by == QLatin1String("artist") || by == QLatin1String("album")) {
        std::stable_sort(rows.begin(), rows.end(), byTitle);
        const bool artistFirst = by == QLatin1String("artist");
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            const Track &x = m_tracks.at(a);
            const Track &y = m_tracks.at(b);
            const int first = artistFirst ? c.compare(sortName(x.filedArtist()), sortName(y.filedArtist())) : c.compare(x.album, y.album);
            if (first != 0) {
                return first < 0;
            }
            const int second = artistFirst ? c.compare(x.album, y.album) : 0;
            if (second != 0) {
                return second < 0;
            }
            if (x.disc != y.disc) {
                return x.disc < y.disc;
            }
            return x.number < y.number;
        });
    } else if (by == QLatin1String("added")) {
        std::stable_sort(rows.begin(), rows.end(), byTitle);
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            return m_tracks.at(a).added > m_tracks.at(b).added;
        });
    } else {
        std::stable_sort(rows.begin(), rows.end(), byTitle);
    }
    return rows;
}

QVariantList Library::mapList(const QList<int> &rows) const
{
    QVariantList list;
    list.reserve(rows.size());
    for (int row : rows) {
        list.append(map(m_tracks.at(row)));
    }
    return list;
}

QVariantList Library::songs(const QString &by) const
{
    QList<int> rows(m_tracks.size());
    std::iota(rows.begin(), rows.end(), 0);
    return mapList(sortedRows(rows, by));
}

QVariantMap Library::albumMap(const Album &a) const
{
    return {
        {QStringLiteral("key"), a.key},
        {QStringLiteral("title"), a.title},
        {QStringLiteral("artist"), a.artist},
        {QStringLiteral("year"), a.year},
        {QStringLiteral("count"), a.tracks.size()},
        {QStringLiteral("duration"), a.duration},
        {QStringLiteral("cover"), a.cover.isEmpty() ? QString() : QUrl::fromLocalFile(QDir(MusicPaths::coverFolder()).filePath(a.cover)).toString()},
    };
}

QVariantList Library::albums(const QString &by) const
{
    QList<const Album *> list;
    list.reserve(m_albums.size());
    for (const Album &a : m_albums) {
        list.append(&a);
    }
    QCollator &c = collator();
    std::sort(list.begin(), list.end(), [&](const Album *x, const Album *y) {
        if (by == QLatin1String("artist")) {
            const int artist = c.compare(sortName(x->artist), sortName(y->artist));
            if (artist != 0) {
                return artist < 0;
            }
            if (x->year != y->year) {
                return x->year < y->year;
            }
        } else if (by == QLatin1String("year") && x->year != y->year) {
            return x->year > y->year;
        } else if (by == QLatin1String("added") && x->added != y->added) {
            return x->added > y->added;
        }
        return c.compare(x->title, y->title) < 0;
    });
    QVariantList out;
    out.reserve(list.size());
    for (const Album *a : list) {
        out.append(albumMap(*a));
    }
    return out;
}

QVariantMap Library::album(const QString &key) const
{
    const auto it = m_albums.constFind(key);
    if (it == m_albums.cend()) {
        return {};
    }
    QVariantMap out = albumMap(*it);
    out.insert(QStringLiteral("tracks"), mapList(it->tracks));
    return out;
}

QVariantList Library::artists() const
{
    QList<const Artist *> list;
    for (const Artist &a : m_artists) {
        list.append(&a);
    }
    QCollator &c = collator();
    std::sort(list.begin(), list.end(), [&](const Artist *x, const Artist *y) {
        return c.compare(sortName(x->name), sortName(y->name)) < 0;
    });
    QVariantList out;
    for (const Artist *a : list) {
        out.append(QVariantMap{
            {QStringLiteral("name"), a->name},
            {QStringLiteral("albums"), a->albums.size()},
            {QStringLiteral("count"), a->tracks.size()},
            {QStringLiteral("cover"), a->cover.isEmpty() ? QString() : QUrl::fromLocalFile(QDir(MusicPaths::coverFolder()).filePath(a->cover)).toString()},
        });
    }
    return out;
}

QVariantMap Library::artist(const QString &name) const
{
    const auto it = m_artists.constFind(name.toLower());
    if (it == m_artists.cend()) {
        return {};
    }
    QVariantList albums;
    QList<const Album *> owned;
    for (const QString &key : it->albums) {
        const auto album = m_albums.constFind(key);
        if (album != m_albums.cend()) {
            owned.append(&*album);
        }
    }
    std::sort(owned.begin(), owned.end(), [](const Album *x, const Album *y) {
        return x->year > y->year;
    });
    for (const Album *a : owned) {
        albums.append(albumMap(*a));
    }
    int duration = 0;
    for (int row : it->tracks) {
        duration += m_tracks.at(row).duration;
    }
    // Newest album first, each in its own order, then songs with no album.
    QList<int> rows;
    for (const Album *a : owned) {
        rows.append(a->tracks);
    }
    QList<int> single;
    for (int row : it->tracks) {
        if (m_tracks.at(row).album.isEmpty()) {
            single.append(row);
        }
    }
    rows.append(sortedRows(single, QStringLiteral("title")));
    return {
        {QStringLiteral("name"), it->name},
        {QStringLiteral("count"), it->tracks.size()},
        {QStringLiteral("duration"), duration},
        {QStringLiteral("cover"), it->cover.isEmpty() ? QString() : QUrl::fromLocalFile(QDir(MusicPaths::coverFolder()).filePath(it->cover)).toString()},
        {QStringLiteral("albums"), albums},
        {QStringLiteral("tracks"), mapList(rows)},
    };
}

QVariantList Library::folder(const QString &path) const
{
    const QDir root(musicFolder());
    const QString base = path.isEmpty() ? root.absolutePath() : root.absoluteFilePath(path);
    const QString prefix = base + QLatin1Char('/');
    QHash<QString, int> folders;
    QList<int> here;
    for (int i = 0; i < m_tracks.size(); ++i) {
        const QString &p = m_tracks.at(i).path;
        if (!p.startsWith(prefix)) {
            continue;
        }
        const qsizetype slash = p.indexOf(QLatin1Char('/'), prefix.size());
        if (slash < 0) {
            here.append(i);
        } else {
            ++folders[p.mid(prefix.size(), slash - prefix.size())];
        }
    }
    QStringList names = folders.keys();
    QCollator &c = collator();
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) {
        return c.compare(a, b) < 0;
    });
    QVariantList out;
    for (const QString &name : names) {
        out.append(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("folder")},
            {QStringLiteral("name"), name},
            {QStringLiteral("path"), path.isEmpty() ? name : path + QLatin1Char('/') + name},
            {QStringLiteral("count"), folders.value(name)},
        });
    }
    for (int row : sortedRows(here, QStringLiteral("number"))) {
        QVariantMap m = map(m_tracks.at(row));
        m.insert(QStringLiteral("kind"), QStringLiteral("song"));
        out.append(m);
    }
    return out;
}

QVariantMap Library::search(const QString &text) const
{
    const QString needle = text.trimmed();
    if (needle.isEmpty()) {
        return {};
    }
    auto has = [&needle](const QString &s) {
        return s.contains(needle, Qt::CaseInsensitive);
    };
    QList<int> rows;
    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &t = m_tracks.at(i);
        if (has(t.title) || has(t.artist) || has(t.album) || has(t.albumArtist)) {
            rows.append(i);
        }
    }
    rows = sortedRows(rows, QStringLiteral("title"));
    if (rows.size() > 100) {
        rows.resize(100);
    }
    QVariantList albums;
    for (const QVariant &a : this->albums(QStringLiteral("title"))) {
        const QVariantMap m = a.toMap();
        if (has(m.value(QStringLiteral("title")).toString()) || has(m.value(QStringLiteral("artist")).toString())) {
            albums.append(m);
        }
    }
    QVariantList artists;
    for (const QVariant &a : this->artists()) {
        if (has(a.toMap().value(QStringLiteral("name")).toString())) {
            artists.append(a);
        }
    }
    return {
        {QStringLiteral("songs"), mapList(rows)},
        {QStringLiteral("albums"), albums.mid(0, 24)},
        {QStringLiteral("artists"), artists.mid(0, 24)},
    };
}

QVariantList Library::recentlyAdded(int limit) const
{
    return albums(QStringLiteral("added")).mid(0, limit);
}

QString Library::lyrics(const QString &path) const
{
    return Tags::lyrics(path);
}

QString Library::albumKeyOf(const QString &path)
{
    const Track *t = find(path);
    return t ? t->albumKey() : QString();
}

void Library::openMusicFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(musicFolder()));
}

void Library::showInFiles(const QString &path) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                                                          QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    message << QStringList{QUrl::fromLocalFile(path).toString()} << QString();
    if (!QDBusConnection::sessionBus().send(message)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }
}
