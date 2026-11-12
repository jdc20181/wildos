#pragma once

#include "track.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include <atomic>

// Every song in the Music folder. The index on disk (MusicPaths::indexFile)
// is read at start so the window fills at once, then a walk in a worker
// thread brings it up to date and the folder is watched for changes.
//
// Songs reach QML as maps: path, title, artist, album, albumKey, year,
// number, disc, duration (ms), cover (a file URL or "") and lyrics.
class Library : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY changed)
    Q_PROPERTY(int albumCount READ albumCount NOTIFY changed)
    Q_PROPERTY(int artistCount READ artistCount NOTIFY changed)
    Q_PROPERTY(QString musicFolder READ musicFolder CONSTANT)

public:
    explicit Library(QObject *parent);
    ~Library() override;
    static Library *instance();
    static Library *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    bool ready() const { return m_ready; }
    bool scanning() const { return m_scanning; }
    int songCount() const { return int(m_tracks.size()); }
    int albumCount() const { return int(m_albums.size()); }
    int artistCount() const { return int(m_artists.size()); }
    QString musicFolder() const;

    // A song in the library, or read from the file for one outside it
    // (opened from Files). Null when the file is not a song.
    const Track *find(const QString &path);
    QVariantMap map(const Track &t) const;
    QString coverUrl(const Track &t) const;

    Q_INVOKABLE QVariantMap track(const QString &path);
    Q_INVOKABLE QVariantList tracks(const QStringList &paths);
    // by: "title", "artist", "album" or "added".
    Q_INVOKABLE QVariantList songs(const QString &by) const;
    Q_INVOKABLE QVariantList albums(const QString &by) const;
    Q_INVOKABLE QVariantMap album(const QString &key) const;
    Q_INVOKABLE QVariantList artists() const;
    Q_INVOKABLE QVariantMap artist(const QString &name) const;
    // The folders and songs directly in a folder under the Music folder;
    // "" is the Music folder itself.
    Q_INVOKABLE QVariantList folder(const QString &path) const;
    Q_INVOKABLE QVariantMap search(const QString &text) const;
    Q_INVOKABLE QVariantList recentlyAdded(int limit) const;
    Q_INVOKABLE QString lyrics(const QString &path) const;
    Q_INVOKABLE QString albumKeyOf(const QString &path);

    Q_INVOKABLE void openMusicFolder() const;
    Q_INVOKABLE void showInFiles(const QString &path) const;
    Q_INVOKABLE void rescan();

Q_SIGNALS:
    void changed();
    void readyChanged();
    void scanningChanged();

private:
    struct Album
    {
        QString key;
        QString title;
        QString artist;
        int year = 0;
        int duration = 0;
        qint64 added = 0;
        QString cover;
        QList<int> tracks;
    };
    struct Artist
    {
        QString name;
        QString cover;
        QStringList albums;
        QList<int> tracks;
    };

    void loadIndex();
    void saveIndex() const;
    void setTracks(const TrackList &tracks);
    void rebuild();
    void startScan();
    void scanFinished(const TrackList &tracks, const QStringList &folders, bool changed);
    void watch(const QStringList &folders);
    QVariantMap albumMap(const Album &a) const;
    QVariantList mapList(const QList<int> &rows) const;
    QList<int> sortedRows(QList<int> rows, const QString &by) const;

    TrackList m_tracks;
    QHash<QString, int> m_byPath;
    QHash<QString, Album> m_albums;
    QHash<QString, Artist> m_artists;
    // Songs opened from outside the Music folder.
    QHash<QString, Track> m_loose;

    int m_revision = 0;
    bool m_ready = false;
    bool m_scanning = false;
    bool m_scanAgain = false;
    QThread *m_thread = nullptr;
    std::atomic_bool m_stop{false};
    QFileSystemWatcher m_watcher;
    QTimer m_rescan;
};
