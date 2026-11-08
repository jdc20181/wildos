#include "preload.h"
#include "store.h"

#include <QFile>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace
{
constexpr qint64 chunk = 256 * 1024;
constexpr qint64 maxPerApp = 256LL * 1024 * 1024;
constexpr qint64 maxBudget = 512LL * 1024 * 1024;
constexpr int maxFiles = 800;
constexpr int topApps = 8;

bool skipped(const QByteArray &path)
{
    static const char *const prefixes[] = {"/dev/", "/proc/", "/sys/", "/run/", "/tmp/", "/memfd:", "/SYSV", "/var/tmp/"};
    for (const char *prefix : prefixes) {
        if (path.startsWith(prefix)) {
            return true;
        }
    }
    return path.endsWith(" (deleted)") || path.contains("/.cache/");
}

// Which chunk of the file has any page in memory.
std::vector<bool> residentChunks(int fd, qint64 size)
{
    std::vector<bool> chunks((size + chunk - 1) / chunk, false);
    if (size <= 0) {
        return chunks;
    }
    void *map = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        return chunks;
    }
    const long page = ::sysconf(_SC_PAGESIZE);
    std::vector<unsigned char> pages((size + page - 1) / page);
    if (::mincore(map, size, pages.data()) == 0) {
        for (size_t i = 0; i < pages.size(); ++i) {
            if (pages[i] & 1) {
                chunks[(qint64(i) * page) / chunk] = true;
            }
        }
    }
    ::munmap(map, size);
    return chunks;
}

qint64 meminfo(const QByteArray &key)
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return -1;
    }
    for (const QByteArray &line : file.readAll().split('\n')) {
        if (line.startsWith(key + ':')) {
            return line.mid(key.size() + 1).trimmed().split(' ').value(0).toLongLong() * 1024;
        }
    }
    return -1;
}

double readValue(const QString &path, const QByteArray &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray text = file.readAll();
    const int at = text.indexOf(key);
    return at < 0 ? 0 : text.mid(at + key.size()).split(' ').value(0).toDouble();
}

// O_NOATIME is refused for files of other owners, which is all of /usr.
int openForReading(const char *path)
{
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC | O_NOATIME);
    return fd >= 0 || errno != EPERM ? fd : ::open(path, O_RDONLY | O_CLOEXEC);
}

double loadAverage()
{
    QFile file(QStringLiteral("/proc/loadavg"));
    return file.open(QIODevice::ReadOnly) ? file.readAll().split(' ').value(0).toDouble() : 0;
}

void lowerPriority()
{
    // Idle I/O class and idle CPU policy: the reads only use the disk and
    // CPU when nothing else wants them, so the desktop never waits.
    constexpr int whoProcess = 1;
    constexpr int classIdle = 3;
    ::syscall(SYS_ioprio_set, whoProcess, 0, classIdle << 13);
    ::setpriority(PRIO_PROCESS, 0, 19);
    sched_param param{};
    ::sched_setscheduler(0, SCHED_IDLE, &param);
}
}

namespace Preload
{
QJsonArray sample(const QList<int> &pids)
{
    QSet<QByteArray> paths;
    for (int pid : pids) {
        QFile maps(QStringLiteral("/proc/%1/maps").arg(pid));
        if (!maps.open(QIODevice::ReadOnly)) {
            continue;
        }
        for (const QByteArray &line : maps.readAll().split('\n')) {
            const int slash = line.indexOf(" /");
            if (slash < 0) {
                continue;
            }
            const QByteArray path = line.mid(slash + 1).trimmed();
            if (!skipped(path)) {
                paths.insert(path);
            }
        }
    }

    QJsonArray files;
    qint64 total = 0;
    for (const QByteArray &path : std::as_const(paths)) {
        if (files.size() >= maxFiles || total >= maxPerApp) {
            break;
        }
        const int fd = openForReading(path.constData());
        if (fd < 0) {
            continue;
        }
        struct stat info{};
        if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
            ::close(fd);
            continue;
        }
        const std::vector<bool> chunks = residentChunks(fd, info.st_size);
        ::close(fd);
        QJsonArray ranges;
        for (size_t i = 0; i < chunks.size();) {
            if (!chunks[i]) {
                ++i;
                continue;
            }
            size_t j = i;
            while (j < chunks.size() && chunks[j]) {
                ++j;
            }
            ranges.append(QJsonArray{qint64(i), qint64(j - i)});
            total += qint64(j - i) * chunk;
            i = j;
        }
        if (!ranges.isEmpty()) {
            files.append(QJsonObject{{QStringLiteral("p"), QString::fromLocal8Bit(path)}, {QStringLiteral("r"), ranges}});
        }
    }
    return files;
}

int run()
{
    QTextStream out(stdout);
    lowerPriority();

    const int delay = qEnvironmentVariableIsSet("WILDOS_PRELOAD_DELAY") ? qEnvironmentVariableIntValue("WILDOS_PRELOAD_DELAY") : 25;
    QThread::sleep(std::chrono::seconds(delay));
    // Wait out the login rush: apps that start themselves, the first
    // indexing. Two minutes at most, then the idle priority is enough.
    const double cores = std::max(1, QThread::idealThreadCount());
    for (int i = 0; i < 24 && loadAverage() > cores * 0.75; ++i) {
        QThread::sleep(std::chrono::seconds(5));
    }

    const qint64 available = meminfo("MemAvailable");
    if (available >= 0 && available < 2LL * 1024 * 1024 * 1024) {
        out << "wildos-launchd: not preloading, memory is short" << Qt::endl;
        return 0;
    }
    const qint64 budget = std::min(maxBudget, available / 4);

    const QJsonObject apps = JsonFile::read(QStringLiteral("usage.json")).value(QStringLiteral("apps")).toObject();
    const QStringList ranked = UsageStore::ranked(apps).mid(0, topApps);
    std::vector<char> buffer(chunk);
    QSet<QString> seen;
    qint64 read = 0;
    qint64 cached = 0;
    for (const QString &id : ranked) {
        const QJsonArray files = apps.value(id).toObject().value(QStringLiteral("files")).toArray();
        for (const QJsonValue &value : files) {
            const QJsonObject entry = value.toObject();
            const QString path = entry.value(QStringLiteral("p")).toString();
            if (seen.contains(path) || read >= budget) {
                continue;
            }
            seen.insert(path);
            // Anything pushing memory out right now matters more.
            if (readValue(QStringLiteral("/proc/pressure/memory"), "some avg10=") > 5.0) {
                out << "wildos-launchd: stopped preloading, memory is under pressure" << Qt::endl;
                return 0;
            }
            const int fd = openForReading(QFile::encodeName(path).constData());
            if (fd < 0) {
                continue;
            }
            struct stat info{};
            if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
                ::close(fd);
                continue;
            }
            const std::vector<bool> resident = residentChunks(fd, info.st_size);
            for (const QJsonValue &r : entry.value(QStringLiteral("r")).toArray()) {
                const qint64 first = r.toArray().at(0).toInteger();
                const qint64 count = r.toArray().at(1).toInteger();
                for (qint64 c = first; c < first + count && c < qint64(resident.size()) && read < budget; ++c) {
                    if (resident[c]) {
                        cached += chunk;
                        continue;
                    }
                    const ssize_t n = ::pread(fd, buffer.data(), chunk, c * chunk);
                    if (n <= 0) {
                        break;
                    }
                    read += n;
                }
            }
            ::close(fd);
        }
    }
    out << "wildos-launchd: preloaded " << read / (1024 * 1024) << " MiB for " << ranked.size() << " apps (" << cached / (1024 * 1024)
        << " MiB were already in memory)" << Qt::endl;
    return 0;
}
}
