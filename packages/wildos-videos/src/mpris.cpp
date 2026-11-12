#include "mpris.h"
#include "player.h"
#include "thumbnails.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMetaProperty>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace
{
const QString objectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString serviceName = QStringLiteral("org.mpris.MediaPlayer2.wildosvideos");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");

qlonglong micro(double seconds)
{
    return qlonglong(std::llround(seconds * 1e6));
}
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
    return QStringLiteral("Videos");
}

QString MprisRoot::desktopEntry() const
{
    return QStringLiteral("org.wildos.videos");
}

QStringList MprisRoot::supportedUriSchemes() const
{
    return {QStringLiteral("file")};
}

QStringList MprisRoot::supportedMimeTypes() const
{
    return {
        QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"), QStringLiteral("video/webm"), QStringLiteral("video/quicktime"),
        QStringLiteral("video/x-msvideo"), QStringLiteral("video/x-ms-wmv"), QStringLiteral("video/mpeg"), QStringLiteral("video/mp2t"),
        QStringLiteral("video/ogg"), QStringLiteral("video/3gpp"),
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
    connect(player, &Player::fileChanged, this, [this] {
        changed({QStringLiteral("Metadata"), QStringLiteral("CanSeek"), QStringLiteral("CanPlay"), QStringLiteral("CanPause")});
    });
    connect(player, &Player::durationChanged, this, [this] {
        changed({QStringLiteral("Metadata")});
    });
    connect(player, &Player::loadedChanged, this, [this] {
        changed({QStringLiteral("CanSeek"), QStringLiteral("PlaybackStatus")});
    });
    connect(player, &Player::folderVideosChanged, this, [this] {
        changed({QStringLiteral("CanGoNext"), QStringLiteral("CanGoPrevious")});
    });
    connect(player, &Player::speedChanged, this, [this] {
        changed({QStringLiteral("Rate")});
    });
    connect(player, &Player::volumeChanged, this, [this] {
        changed({QStringLiteral("Volume")});
    });
    connect(player, &Player::mutedChanged, this, [this] {
        changed({QStringLiteral("Volume")});
    });
    // The still appears once it is made.
    connect(Thumbnails::instance(), &Thumbnails::ready, this, [this](const QString &path) {
        if (path == Player::instance()->path()) {
            changed({QStringLiteral("Metadata")});
        }
    });
    // Position is never sent as a change (MPRIS says listeners work it
    // out from the rate); a jump is announced with Seeked instead.
    connect(player, &Player::seeked, this, [this](double position) {
        Q_EMIT Seeked(micro(position));
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
    return player->loaded() ? QStringLiteral("Paused") : QStringLiteral("Stopped");
}

double MprisPlayer::rate() const
{
    return Player::instance()->speed();
}

void MprisPlayer::setRate(double rate)
{
    // MPRIS says a rate of 0 means pause.
    if (rate <= 0) {
        Player::instance()->pause();
        return;
    }
    Player::instance()->setSpeed(rate);
}

QDBusObjectPath MprisPlayer::trackId() const
{
    const QString path = Player::instance()->path();
    if (path.isEmpty()) {
        return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    }
    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex();
    return QDBusObjectPath(QStringLiteral("/org/wildos/videos/video/") + QString::fromLatin1(hash));
}

QVariantMap MprisPlayer::metadata() const
{
    Player *player = Player::instance();
    if (player->path().isEmpty()) {
        return {};
    }
    QVariantMap m{
        {QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId())},
        {QStringLiteral("mpris:length"), micro(player->duration())},
        {QStringLiteral("xesam:title"), player->title()},
        {QStringLiteral("xesam:url"), QUrl::fromLocalFile(player->path()).toString()},
    };
    const QString place = player->place();
    if (!place.isEmpty()) {
        m.insert(QStringLiteral("xesam:album"), place);
    }
    const QString image = Thumbnails::instance()->info(player->path()).value(QStringLiteral("image")).toString();
    if (!image.isEmpty()) {
        m.insert(QStringLiteral("mpris:artUrl"), image);
    }
    return m;
}

double MprisPlayer::volume() const
{
    Player *player = Player::instance();
    return player->muted() ? 0.0 : player->volume() / 100.0;
}

void MprisPlayer::setVolume(double volume)
{
    Player *player = Player::instance();
    player->setMuted(false);
    player->setVolume(int(std::lround(std::clamp(volume, 0.0, 1.0) * 100)));
}

qlonglong MprisPlayer::position() const
{
    return micro(Player::instance()->position());
}

bool MprisPlayer::canGoNext() const
{
    return Player::instance()->canNext();
}

bool MprisPlayer::canGoPrevious() const
{
    return Player::instance()->loaded();
}

bool MprisPlayer::canPlay() const
{
    return !Player::instance()->path().isEmpty();
}

bool MprisPlayer::canSeek() const
{
    return Player::instance()->loaded();
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
    const double to = player->position() + offset / 1e6;
    if (player->duration() > 0 && to >= player->duration()) {
        player->next();
        return;
    }
    player->seek(std::max(0.0, to));
}

void MprisPlayer::SetPosition(const QDBusObjectPath &track, qlonglong position)
{
    Player *player = Player::instance();
    if (track != trackId() || position < 0 || position / 1e6 > player->duration()) {
        return;
    }
    player->seek(position / 1e6);
}

void MprisPlayer::OpenUri(const QString &uri)
{
    Player::instance()->open(uri);
}
