#include "tags.h"
#include "paths.h"

#include <audioproperties.h>
#include <fileref.h>
#include <tpropertymap.h>
#include <tvariant.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSaveFile>

namespace
{
QString text(const TagLib::PropertyMap &properties, const char *key)
{
    const auto it = properties.find(key);
    if (it == properties.end() || it->second.isEmpty()) {
        return QString();
    }
    return QString::fromStdString(it->second.front().to8Bit(true)).trimmed();
}

// "3/11" and "3" both mean the third.
int leadingNumber(const QString &value)
{
    int n = 0;
    for (const QChar c : value) {
        if (!c.isDigit()) {
            break;
        }
        n = n * 10 + c.digitValue();
    }
    return n;
}

TagLib::FileRef open(const QString &path, bool lengths)
{
    return TagLib::FileRef(QFile::encodeName(path).constData(), lengths, TagLib::AudioProperties::Average);
}

// Picks the front cover when a file carries several pictures.
QByteArray picture(const TagLib::FileRef &ref)
{
    const TagLib::List<TagLib::VariantMap> pictures = ref.complexProperties("PICTURE");
    QByteArray first;
    for (const TagLib::VariantMap &p : pictures) {
        const TagLib::ByteVector data = p.value("data").toByteVector();
        if (data.isEmpty()) {
            continue;
        }
        const QByteArray bytes(data.data(), qsizetype(data.size()));
        if (p.value("pictureType").toString() == "Front Cover") {
            return bytes;
        }
        if (first.isEmpty()) {
            first = bytes;
        }
    }
    return first;
}
}

const QStringList &Tags::extensions()
{
    static const QStringList list{
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
        QStringLiteral("m4a"), QStringLiteral("m4b"), QStringLiteral("mp4"), QStringLiteral("aac"), QStringLiteral("alac"),
        QStringLiteral("wav"), QStringLiteral("wave"), QStringLiteral("aif"), QStringLiteral("aiff"), QStringLiteral("wma"),
        QStringLiteral("ape"), QStringLiteral("wv"), QStringLiteral("mpc"),
    };
    return list;
}

bool Tags::isAudio(const QString &path)
{
    const qsizetype dot = path.lastIndexOf(QLatin1Char('.'));
    return dot > 0 && extensions().contains(path.mid(dot + 1).toLower());
}

Track Tags::read(const QFileInfo &file)
{
    Track t;
    t.path = file.absoluteFilePath();
    t.modified = file.lastModified().toSecsSinceEpoch();
    t.size = file.size();

    const TagLib::FileRef ref = open(t.path, true);
    if (!ref.isNull()) {
        const TagLib::PropertyMap properties = ref.properties();
        t.title = text(properties, "TITLE");
        t.artist = text(properties, "ARTIST");
        t.album = text(properties, "ALBUM");
        t.albumArtist = text(properties, "ALBUMARTIST");
        t.genre = text(properties, "GENRE");
        t.year = leadingNumber(text(properties, "DATE"));
        t.number = leadingNumber(text(properties, "TRACKNUMBER"));
        t.disc = leadingNumber(text(properties, "DISCNUMBER"));
        t.lyrics = !text(properties, "LYRICS").isEmpty();
        if (const TagLib::AudioProperties *audio = ref.audioProperties()) {
            t.duration = audio->lengthInMilliseconds();
        }
        t.cover = storeCover(picture(ref));
    }
    if (t.title.isEmpty()) {
        t.title = file.completeBaseName();
    }
    return t;
}

QString Tags::lyrics(const QString &path)
{
    const TagLib::FileRef ref = open(path, false);
    if (ref.isNull()) {
        return QString();
    }
    const TagLib::PropertyMap properties = ref.properties();
    const auto it = properties.find("LYRICS");
    if (it == properties.end() || it->second.isEmpty()) {
        return QString();
    }
    return QString::fromStdString(it->second.front().to8Bit(true)).trimmed();
}

QString Tags::storeCover(const QByteArray &data)
{
    if (data.isEmpty()) {
        return QString();
    }
    const QString name = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(20)) + QStringLiteral(".jpg");
    const QDir folder(MusicPaths::coverFolder());
    if (QFile::exists(folder.filePath(name))) {
        return name;
    }
    QImage image = QImage::fromData(data);
    if (image.isNull()) {
        return QString();
    }
    // Large enough for the album page's 200px cover on a 2x screen.
    if (image.width() > 480 || image.height() > 480) {
        image = image.scaled(480, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    folder.mkpath(QStringLiteral("."));
    QSaveFile out(folder.filePath(name));
    if (!out.open(QIODevice::WriteOnly) || !image.convertToFormat(QImage::Format_RGB32).save(&out, "JPG", 88) || !out.commit()) {
        return QString();
    }
    return name;
}
