#include "sizes.h"
#include "paths.h"
#include "plan.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QThread>

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
// Numbers younger than this are shown as they are.
constexpr qint64 freshFor = 6 * 3600;

struct Walk {
    QSet<QByteArray> skip;
    QByteArray varApp;
    const std::atomic_bool *cancel = nullptr;

    // Folders restic leaves out under --exclude-caches.
    static bool cacheTagged(int dirFd)
    {
        const int fd = ::openat(dirFd, "CACHEDIR.TAG", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        static const char signature[] = "Signature: 8a477f597d28d172789f06886806bc55";
        char buffer[sizeof(signature)] = {};
        const ssize_t n = ::read(fd, buffer, sizeof(signature) - 1);
        ::close(fd);
        return n == ssize_t(sizeof(signature) - 1) && std::memcmp(buffer, signature, sizeof(signature) - 1) == 0;
    }

    qint64 dir(const QByteArray &path)
    {
        if (cancel->load() || skip.contains(path)) {
            return 0;
        }
        if (path.startsWith(varApp) && path.endsWith("/cache") && path.count('/') == varApp.count('/') + 1) {
            return 0;
        }
        const int fd = ::open(path.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            return 0;
        }
        if (cacheTagged(fd)) {
            ::close(fd);
            return 0;
        }
        DIR *d = ::fdopendir(fd);
        if (!d) {
            ::close(fd);
            return 0;
        }
        qint64 total = 0;
        QList<QByteArray> children;
        while (dirent *e = ::readdir(d)) {
            if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) {
                continue;
            }
            struct stat st;
            if (::fstatat(fd, e->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
                continue;
            }
            if (S_ISDIR(st.st_mode)) {
                children.append(path + '/' + e->d_name);
            } else if (S_ISREG(st.st_mode)) {
                total += st.st_size;
            }
        }
        ::closedir(d);
        for (const QByteArray &child : std::as_const(children)) {
            total += dir(child);
        }
        return total;
    }
};
}

Sizes::Sizes(QObject *parent)
    : QObject(parent)
{
    loadCache();
    m_settle.setSingleShot(true);
    m_settle.setInterval(1500);
    connect(&m_settle, &QTimer::timeout, this, [this] { refresh(true); });
    // Leaving a folder out changes what the home folder holds.
    connect(Settings::instance(), &Settings::includeChanged, this, [this] {
        if (key() != m_cacheKey) {
            m_settle.start();
        }
    });
}

Sizes::~Sizes()
{
    m_cancel = true;
    if (m_thread) {
        m_thread->wait();
    }
}

QString Sizes::key() const
{
    return Settings::instance()->excludes().join(QLatin1Char('\n'));
}

void Sizes::loadCache()
{
    QFile f(Paths::sizesCache());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_home = o.value(QStringLiteral("home")).toDouble(-1);
    m_mail = o.value(QStringLiteral("mail")).toDouble(-1);
    m_settings = o.value(QStringLiteral("settings")).toDouble(-1);
    m_at = QDateTime::fromString(o.value(QStringLiteral("at")).toString(), Qt::ISODate);
    m_cacheKey = o.value(QStringLiteral("key")).toString();
}

void Sizes::refresh(bool force)
{
    if (m_working) {
        m_again = true;
        return;
    }
    const bool fresh = m_at.isValid() && m_at.secsTo(QDateTime::currentDateTime()) < freshFor && m_cacheKey == key();
    if (fresh && !force) {
        return;
    }
    m_working = true;
    Q_EMIT changed();

    const QString home = Paths::home();
    QSet<QByteArray> out;
    for (const QString &p : Plan::alwaysOut()) {
        out.insert(QFile::encodeName(p));
    }
    for (const QString &p : Settings::instance()->excludes()) {
        out.insert(QFile::encodeName(QDir::cleanPath(p)));
    }
    QList<QByteArray> mailPaths;
    for (const QString &p : Plan::mailPaths()) {
        mailPaths.append(QFile::encodeName(p));
    }
    QList<QByteArray> settingsPaths;
    for (const QString &p : Plan::settingsPaths()) {
        settingsPaths.append(QFile::encodeName(p));
    }
    const QByteArray varApp = QFile::encodeName(home + QStringLiteral("/.var/app"));
    const QByteArray homeBytes = QFile::encodeName(home);
    const QString cacheKey = key();

    m_thread = QThread::create([this, out, mailPaths, settingsPaths, varApp, homeBytes, cacheKey]() {
        Walk walk;
        walk.varApp = varApp;
        walk.cancel = &m_cancel;

        walk.skip = out;
        for (const QByteArray &p : settingsPaths) {
            walk.skip.insert(p);
        }
        const double homeSize = double(walk.dir(homeBytes));

        walk.skip = out;
        double mailSize = 0;
        for (const QByteArray &p : mailPaths) {
            mailSize += double(walk.dir(p));
        }

        walk.skip = out;
        for (const QByteArray &p : mailPaths) {
            walk.skip.insert(p);
        }
        double settingsSize = 0;
        for (const QByteArray &p : settingsPaths) {
            settingsSize += double(walk.dir(p));
        }
        QMetaObject::invokeMethod(this, [this, homeSize, mailSize, settingsSize, cacheKey] { done(homeSize, mailSize, settingsSize, cacheKey); },
                                  Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start(QThread::LowPriority);
}

void Sizes::done(double home, double mail, double settings, const QString &cacheKey)
{
    m_thread = nullptr;
    m_working = false;
    if (m_cancel) {
        return;
    }
    m_home = home;
    m_mail = mail;
    m_settings = settings;
    m_at = QDateTime::currentDateTime();
    m_cacheKey = cacheKey;
    const QString path = Paths::sizesCache();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{
                                  {QStringLiteral("home"), home},
                                  {QStringLiteral("mail"), mail},
                                  {QStringLiteral("settings"), settings},
                                  {QStringLiteral("at"), m_at.toString(Qt::ISODate)},
                                  {QStringLiteral("key"), cacheKey},
                              })
                    .toJson(QJsonDocument::Compact));
        f.commit();
    }
    Q_EMIT changed();
    if (m_again) {
        m_again = false;
        refresh(true);
    }
}
