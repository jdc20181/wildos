#include "mpris.h"
#include "player.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMetaProperty>
#include <QUrl>

#include <algorithm>

namespace
{
const QString objectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString serviceName = QStringLiteral("org.mpris.MediaPlayer2.wildosmusic");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
}

Mpris::Mpris(QObject *parent)
    : QObject(parent)
{
    new MprisRoot(this);
    new MprisPlayer(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors);
    bus.registerService(serviceName);
}

Mpris::~Mpris()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterService(serviceName);
    bus.unregisterObject(objectPath);
}

void Mpris::notify(const QString &interface, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(objectPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
    signal << interface << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

MprisRoot::MprisRoot(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
}

QString MprisRoot::identity() const
{
    return QStringLiteral("Music");
}

QString MprisRoot::desktopEntry() const
{
    return QStringLiteral("org.wildos.music");
}

QStringList MprisRoot::supportedUriSchemes() const
{
    return {QStringLiteral("file")};
}

QStringList MprisRoot::supportedMimeTypes() const
{
    return {
        QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"), QStringLiteral("audio/ogg"), QStringLiteral("audio/x-vorbis+ogg"),
        QStringLiteral("audio/x-opus+ogg"), QStringLiteral("audio/mp4"), QStringLiteral("audio/aac"), QStringLiteral("audio/x-wav"),
        QStringLiteral("audio/x-aiff"), QStringLiteral("audio/x-ms-wma"),
    };
}

void MprisRoot::Raise()
{
    Q_EMIT m_mpris->raiseRequested();
}

void MprisRoot::Quit()
{
    QCoreApplication::quit();
}

MprisPlayer::MprisPlayer(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
    Player *player = Player::instance();
    connect(player, &Player::playingChanged, this, [this] {
        changed({QStringLiteral("PlaybackStatus"), QStringLiteral("CanPlay"), QStringLiteral("CanPause")});
    });
    connect(player, &Player::currentChanged, this, [this] {
        changed({QStringLiteral("Metadata"), QStringLiteral("CanSeek")});
    });
    connect(player, &Player::durationChanged, this, [this] {
        changed({QStringLiteral("Metadata")});
    });
    connect(player, &Player::seekableChanged, this, [this] {
        changed({QStringLiteral("CanSeek")});
    });
    connect(player, &Player::queueChanged, this, [this] {
        changed({QStringLiteral("CanGoNext"), QStringLiteral("CanGoPrevious"), QStringLiteral("CanPlay"), QStringLiteral("CanPause")});
    });
    connect(player, &Player::repeatChanged, this, [this] {
        changed({QStringLiteral("LoopStatus")});
    });
    connect(player, &Player::shuffleChanged, this, [this] {
        changed({QStringLiteral("Shuffle")});
    });
    connect(player, &Player::volumeChanged, this, [this] {
        changed({QStringLiteral("Volume")});
    });
    // Position is never sent as a change (MPRIS says listeners work it
    // out from the rate); a jump is announced with Seeked instead.
    connect(player, &Player::seeked, this, [this](qint64 position) {
        Q_EMIT Seeked(position * 1000);
    });
}

void MprisPlayer::changed(const QStringList &properties)
{
    QVariantMap values;
    for (const QString &name : properties) {
        const int index = metaObject()->indexOfProperty(name.toLatin1().constData());
        if (index >= 0) {
            values.insert(name, metaObject()->property(index).read(this));
        }
    }
    m_mpris->notify(playerInterface, values);
}

QString MprisPlayer::playbackStatus() const
{
    Player *player = Player::instance();
    if (player->playing()) {
        return QStringLiteral("Playing");
    }
    return player->currentPath().isEmpty() ? QStringLiteral("Stopped") : QStringLiteral("Paused");
}

QString MprisPlayer::loopStatus() const
{
    const QString repeat = Player::instance()->repeat();
    return repeat == QLatin1String("one") ? QStringLiteral("Track") : repeat == QLatin1String("all") ? QStringLiteral("Playlist") : QStringLiteral("None");
}

void MprisPlayer::setLoopStatus(const QString &status)
{
    Player::instance()->setRepeat(status == QLatin1String("Track") ? QStringLiteral("one") : status == QLatin1String("Playlist") ? QStringLiteral("all") : QStringLiteral("off"));
}

bool MprisPlayer::shuffle() const
{
    return Player::instance()->shuffle();
}

void MprisPlayer::setShuffle(bool on)
{
    Player::instance()->setShuffle(on);
}

QDBusObjectPath MprisPlayer::trackId() const
{
    const int index = Player::instance()->index();
    return QDBusObjectPath(index >= 0 ? QStringLiteral("/org/wildos/music/queue/%1").arg(index) : QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
}

QVariantMap MprisPlayer::metadata() const
{
    Player *player = Player::instance();
    const QVariantMap current = player->current();
    if (current.isEmpty()) {
        return {};
    }
    QVariantMap m{
        {QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId())},
        {QStringLiteral("mpris:length"), qlonglong(player->duration()) * 1000},
        {QStringLiteral("xesam:title"), current.value(QStringLiteral("title")).toString()},
        {QStringLiteral("xesam:artist"), QStringList{current.value(QStringLiteral("artist")).toString()}},
        {QStringLiteral("xesam:url"), QUrl::fromLocalFile(current.value(QStringLiteral("path")).toString()).toString()},
    };
    const QString album = current.value(QStringLiteral("album")).toString();
    if (!album.isEmpty()) {
        m.insert(QStringLiteral("xesam:album"), album);
        m.insert(QStringLiteral("xesam:albumArtist"), QStringList{current.value(QStringLiteral("albumArtist")).toString()});
    }
    if (const int number = current.value(QStringLiteral("number")).toInt()) {
        m.insert(QStringLiteral("xesam:trackNumber"), number);
    }
    const QString cover = current.value(QStringLiteral("cover")).toString();
    if (!cover.isEmpty()) {
        m.insert(QStringLiteral("mpris:artUrl"), cover);
    }
    return m;
}

double MprisPlayer::volume() const
{
    Player *player = Player::instance();
    return player->muted() ? 0.0 : player->volume();
}

void MprisPlayer::setVolume(double volume)
{
    Player *player = Player::instance();
    player->setMuted(false);
    player->setVolume(volume);
}

qlonglong MprisPlayer::position() const
{
    return Player::instance()->position() * 1000;
}

bool MprisPlayer::canGoNext() const
{
    return Player::instance()->canNext();
}

bool MprisPlayer::canGoPrevious() const
{
    return Player::instance()->canPrevious();
}

bool MprisPlayer::canPlay() const
{
    return !Player::instance()->queue().isEmpty();
}

bool MprisPlayer::canSeek() const
{
    return Player::instance()->seekable();
}

void MprisPlayer::Next()
{
    Player::instance()->next();
}

void MprisPlayer::Previous()
{
    Player::instance()->previous();
}

void MprisPlayer::Pause()
{
    Player::instance()->pause();
}

void MprisPlayer::PlayPause()
{
    Player::instance()->playPause();
}

void MprisPlayer::Stop()
{
    Player::instance()->stop();
}

void MprisPlayer::Play()
{
    Player::instance()->play();
}

void MprisPlayer::Seek(qlonglong offset)
{
    Player *player = Player::instance();
    const qint64 to = player->position() + offset / 1000;
    if (player->duration() > 0 && to >= player->duration()) {
        player->next();
        return;
    }
    player->seek(std::max<qint64>(0, to));
}

void MprisPlayer::SetPosition(const QDBusObjectPath &track, qlonglong position)
{
    Player *player = Player::instance();
    if (track != trackId() || position < 0 || position / 1000 > player->duration()) {
        return;
    }
    player->seek(position / 1000);
}

void MprisPlayer::OpenUri(const QString &uri)
{
    Player::instance()->openFiles({uri});
}
