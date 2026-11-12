#pragma once

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

// One song as the index keeps it: what the tags say, with the file's name
// standing in for a missing title.
struct Track
{
    QString path;
    qint64 modified = 0;
    qint64 size = 0;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    int year = 0;
    int number = 0;
    int disc = 0;
    // Milliseconds; 0 when the tags could not say.
    int duration = 0;
    // A file in MusicPaths::coverFolder(), or empty for no art.
    QString cover;
    bool lyrics = false;
    // When this song first turned up in the Music folder, in seconds.
    qint64 added = 0;

    QJsonObject toJson() const
    {
        QJsonObject o{
            {QStringLiteral("p"), path},
            {QStringLiteral("m"), modified},
            {QStringLiteral("s"), size},
            {QStringLiteral("t"), title},
            {QStringLiteral("a"), added},
        };
        auto put = [&o](const char *key, const QString &value) {
            if (!value.isEmpty()) {
                o.insert(QLatin1String(key), value);
            }
        };
        put("ar", artist);
        put("al", album);
        put("aa", albumArtist);
        put("g", genre);
        put("c", cover);
        if (year) {
            o.insert(QStringLiteral("y"), year);
        }
        if (number) {
            o.insert(QStringLiteral("n"), number);
        }
        if (disc) {
            o.insert(QStringLiteral("dn"), disc);
        }
        if (duration) {
            o.insert(QStringLiteral("d"), duration);
        }
        if (lyrics) {
            o.insert(QStringLiteral("l"), true);
        }
        return o;
    }

    static Track fromJson(const QJsonObject &o)
    {
        Track t;
        t.path = o.value(QLatin1String("p")).toString();
        t.modified = o.value(QLatin1String("m")).toInteger();
        t.size = o.value(QLatin1String("s")).toInteger();
        t.title = o.value(QLatin1String("t")).toString();
        t.artist = o.value(QLatin1String("ar")).toString();
        t.album = o.value(QLatin1String("al")).toString();
        t.albumArtist = o.value(QLatin1String("aa")).toString();
        t.genre = o.value(QLatin1String("g")).toString();
        t.cover = o.value(QLatin1String("c")).toString();
        t.year = o.value(QLatin1String("y")).toInt();
        t.number = o.value(QLatin1String("n")).toInt();
        t.disc = o.value(QLatin1String("dn")).toInt();
        t.duration = o.value(QLatin1String("d")).toInt();
        t.lyrics = o.value(QLatin1String("l")).toBool();
        t.added = o.value(QLatin1String("a")).toInteger();
        return t;
    }

    // The artist an album is filed under: compilations name an album
    // artist, everything else goes by the song's own artist.
    QString filedArtist() const
    {
        return albumArtist.isEmpty() ? artist : albumArtist;
    }
    QString albumKey() const
    {
        return album.isEmpty() ? QString() : filedArtist().toLower() + QChar(0x1f) + album.toLower();
    }
};

using TrackList = QList<Track>;
Q_DECLARE_METATYPE(Track)
Q_DECLARE_METATYPE(TrackList)
