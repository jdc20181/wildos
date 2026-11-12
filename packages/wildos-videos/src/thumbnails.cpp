#include "thumbnails.h"
#include "paths.h"

#include <mpv/client.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRunnable>
#include <QSaveFile>
#include <QUrl>

#include <atomic>

namespace
{
Thumbnails *s_instance = nullptr;
std::atomic_bool s_quitting = false;

// Version 1: a 512 by 288 cover crop, a tenth of the way in.
constexpr int cacheVersion = 1;

QString cacheKey(const QFileInfo &file)
{
    const QByteArray seed = file.absoluteFilePath().toUtf8() + '\n' + QByteArray::number(file.size()) + '\n'
        + QByteArray::number(file.lastModified().toMSecsSinceEpoch()) + '\n' + QByteArray::number(cacheVersion);
    return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha1).toHex());
}

QVariantMap readMeta(const QString &metaPath, const QString &imagePath)
{
    QFile file(metaPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    if (o.isEmpty()) {
        return {};
    }
    QVariantMap info = o.toVariantMap();
    info.insert(QStringLiteral("image"), QFileInfo::exists(imagePath) ? QUrl::fromLocalFile(imagePath).toString() : QString());
    info.insert(QStringLiteral("ready"), true);
    return info;
}

class Job : public QRunnable
{
public:
    explicit Job(const QString &path)
        : m_path(path)
    {
    }

    void run() override
    {
        if (s_quitting) {
            return;
        }
        const QFileInfo file(m_path);
        const QDir cache(VideosPaths::thumbnailFolder());
        const QString key = cacheKey(file);
        const QString imagePath = cache.filePath(key + QStringLiteral(".jpg"));
        const QString metaPath = cache.filePath(key + QStringLiteral(".json"));
        QVariantMap info = readMeta(metaPath, imagePath);
        if (info.isEmpty()) {
            info = make(cache, key, imagePath, metaPath);
        }
        if (s_quitting || !s_instance) {
            return;
        }
        QMetaObject::invokeMethod(s_instance, "finish", Qt::QueuedConnection, Q_ARG(QString, m_path), Q_ARG(QVariantMap, info));
    }

private:
    QVariantMap make(const QDir &cache, const QString &key, const QString &imagePath, const QString &metaPath)
    {
        cache.mkpath(QStringLiteral("."));
        const QString workFolder = cache.filePath(QStringLiteral("work-") + key);
        QDir(workFolder).removeRecursively();
        cache.mkpath(workFolder);

        double duration = 0;
        qint64 width = 0;
        qint64 height = 0;
        if (mpv_handle *mpv = mpv_create()) {
            // Nothing is read but the file: no scripts, no config, no
            // online lookups, no sound and no subtitles.
            const char *const options[][2] = {
                {"config", "no"},
                {"load-scripts", "no"},
                {"ytdl", "no"},
                {"terminal", "no"},
                {"msg-level", "all=no"},
                {"vo", "image"},
                {"vo-image-format", "jpg"},
                {"vo-image-jpeg-quality", "85"},
                {"ao", "null"},
                {"aid", "no"},
                {"sid", "no"},
                {"hwdec", "no"},
                {"vd-lavc-threads", "2"},
                {"frames", "1"},
                {"start", "10%"},
                {"audio-display", "no"},
                {"vf", "scale=w=512:h=288:force_original_aspect_ratio=increase,crop=w=512:h=288"},
            };
            for (const auto &option : options) {
                mpv_set_option_string(mpv, option[0], option[1]);
            }
            mpv_set_option_string(mpv, "vo-image-outdir", QFile::encodeName(workFolder).constData());
            if (mpv_initialize(mpv) >= 0) {
                const QByteArray path = QFile::encodeName(m_path);
                const char *command[] = {"loadfile", path.constData(), nullptr};
                mpv_command(mpv, command);
                QElapsedTimer clock;
                clock.start();
                // A broken file must not hold a worker for long.
                while (!s_quitting && clock.elapsed() < 20000) {
                    mpv_event *event = mpv_wait_event(mpv, 0.25);
                    if (event->event_id == MPV_EVENT_FILE_LOADED) {
                        mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration);
                        mpv_get_property(mpv, "width", MPV_FORMAT_INT64, &width);
                        mpv_get_property(mpv, "height", MPV_FORMAT_INT64, &height);
                    } else if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN) {
                        break;
                    }
                }
            }
            mpv_terminate_destroy(mpv);
        }

        const QStringList frames = QDir(workFolder).entryList({QStringLiteral("*.jpg")}, QDir::Files, QDir::Name);
        QFile::remove(imagePath);
        if (!frames.isEmpty()) {
            QFile::rename(QDir(workFolder).filePath(frames.first()), imagePath);
        }
        QDir(workFolder).removeRecursively();

        QVariantMap info{
            {QStringLiteral("duration"), duration},
            {QStringLiteral("width"), width},
            {QStringLiteral("height"), height},
        };
        if (!s_quitting) {
            QSaveFile meta(metaPath);
            if (meta.open(QIODevice::WriteOnly)) {
                meta.write(QJsonDocument(QJsonObject::fromVariantMap(info)).toJson(QJsonDocument::Compact));
                meta.commit();
            }
        }
        info.insert(QStringLiteral("image"), QFileInfo::exists(imagePath) ? QUrl::fromLocalFile(imagePath).toString() : QString());
        info.insert(QStringLiteral("ready"), true);
        return info;
    }

    QString m_path;
};
}

Thumbnails::Thumbnails(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    // Two at a time: quick enough for a folder, and the picture that is
    // playing keeps the rest of the processor.
    m_pool.setMaxThreadCount(2);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        s_quitting = true;
        m_pool.clear();
    });
}

Thumbnails::~Thumbnails()
{
    s_quitting = true;
    m_pool.clear();
    m_pool.waitForDone();
    s_instance = nullptr;
}

Thumbnails *Thumbnails::instance()
{
    if (!s_instance) {
        new Thumbnails(QCoreApplication::instance());
    }
    return s_instance;
}

Thumbnails *Thumbnails::create(QQmlEngine *, QJSEngine *)
{
    Thumbnails *thumbnails = instance();
    QQmlEngine::setObjectOwnership(thumbnails, QQmlEngine::CppOwnership);
    return thumbnails;
}

QVariantMap Thumbnails::info(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }
    const auto it = m_known.constFind(path);
    if (it != m_known.cend()) {
        return *it;
    }
    if (!m_pending.contains(path)) {
        m_pending.insert(path);
        m_pool.start(new Job(path));
    }
    return {{QStringLiteral("ready"), false}};
}

void Thumbnails::finish(const QString &path, const QVariantMap &info)
{
    m_pending.remove(path);
    m_known.insert(path, info);
    Q_EMIT ready(path, info);
}
