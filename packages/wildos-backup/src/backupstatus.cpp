#include "backupstatus.h"
#include "history.h"
#include "paths.h"
#include "schedule.h"
#include "settings.h"
#include "sizes.h"
#include "windowsmode.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QThread>

#include <csignal>
#include <sys/types.h>

namespace
{
BackupStatus *s_instance = nullptr;

// Only a backup run of ours, never whatever process now has its old number.
bool isRunner(qint64 pid)
{
    if (pid <= 0 || ::kill(static_cast<pid_t>(pid), 0) != 0) {
        return false;
    }
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) {
        return false;
    }
    // "pid (comm) state ...": a run that ended but wasn't reaped yet is Z.
    const QByteArray line = stat.readAll();
    const qsizetype open = line.indexOf('(');
    const qsizetype close = line.lastIndexOf(')');
    return open > 0 && close > open && line.mid(open + 1, close - open - 1).startsWith("wildos-backup") && line.mid(close + 2, 1) != "Z";
}
}

BackupStatus::BackupStatus(QObject *parent)
    : QObject(parent)
    , m_sizes(new Sizes(this))
{
    s_instance = this;
    loadHistory();
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &BackupStatus::poll);
    m_poll.start();
    poll();
    // "Next backup" moves on as time passes.
    m_minute.setInterval(60000);
    connect(&m_minute, &QTimer::timeout, this, &BackupStatus::nextRunChanged);
    m_minute.start();
    connect(Settings::instance(), &Settings::scheduleChanged, this, &BackupStatus::nextRunChanged);
    refreshWindows();
}

BackupStatus *BackupStatus::instance()
{
    if (!s_instance) {
        new BackupStatus(QCoreApplication::instance());
    }
    return s_instance;
}

BackupStatus *BackupStatus::create(QQmlEngine *, QJSEngine *)
{
    BackupStatus *status = instance();
    QQmlEngine::setObjectOwnership(status, QQmlEngine::CppOwnership);
    return status;
}

QString BackupStatus::phase() const
{
    return m_status.value(QStringLiteral("phase"), QStringLiteral("preparing")).toString();
}
double BackupStatus::percent() const
{
    return m_status.value(QStringLiteral("percent"), -1).toDouble();
}
double BackupStatus::filesDone() const
{
    return m_status.value(QStringLiteral("filesDone"), 0).toDouble();
}
double BackupStatus::totalFiles() const
{
    return m_status.value(QStringLiteral("totalFiles"), 0).toDouble();
}
double BackupStatus::bytesDone() const
{
    return m_status.value(QStringLiteral("bytesDone"), 0).toDouble();
}
double BackupStatus::totalBytes() const
{
    return m_status.value(QStringLiteral("totalBytes"), 0).toDouble();
}
double BackupStatus::secondsLeft() const
{
    return m_status.value(QStringLiteral("secondsLeft"), -1).toDouble();
}
QString BackupStatus::runDrive() const
{
    return m_status.value(QStringLiteral("drive")).toString();
}

QVariantMap BackupStatus::last() const
{
    return m_history.isEmpty() ? QVariantMap() : m_history.first().toMap();
}

QVariantMap BackupStatus::lastDone() const
{
    for (const QVariant &v : m_history) {
        const QVariantMap e = v.toMap();
        if (e.value(QStringLiteral("result")).toString() == QLatin1String("done")) {
            return e;
        }
    }
    return {};
}

QDateTime BackupStatus::nextRun() const
{
    return Settings::instance()->nextRun(QDateTime::currentDateTime());
}

void BackupStatus::loadHistory()
{
    m_history = History::load().toVariantList();
    m_pending = History::pendingSince();
    m_historyStamp = QFileInfo(Paths::historyFile()).lastModified();
}

void BackupStatus::poll()
{
    const QFileInfo history(Paths::historyFile());
    const QFileInfo pending(Paths::pendingFile());
    if (history.lastModified() != m_historyStamp || pending.exists() != m_pending.isValid()) {
        loadHistory();
        Q_EMIT historyChanged();
    }

    const QFileInfo statusInfo(Paths::statusFile());
    bool running = false;
    if (statusInfo.exists()) {
        if (statusInfo.lastModified() != m_statusStamp) {
            m_statusStamp = statusInfo.lastModified();
            QFile f(Paths::statusFile());
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
                if (!o.isEmpty()) {
                    m_status = o.toVariantMap();
                    m_pid = static_cast<qint64>(o.value(QStringLiteral("pid")).toDouble());
                }
            }
        }
        running = isRunner(m_pid);
        if (running) {
            m_starting = false;
        }
    }
    if (m_starting) {
        running = true;
    }

    const bool ended = m_running && !running;
    if (running != m_running || running) {
        m_running = running;
        if (!running) {
            m_status.clear();
            m_stopping = false;
        }
        Q_EMIT progressChanged();
    }
    if (ended) {
        // The run noted the drive's last backup in the shared settings file.
        Settings::instance()->reload();
        loadHistory();
        Q_EMIT historyChanged();
        Q_EMIT runEnded(last());
        refreshWindows();
    }
}

void BackupStatus::backUpNow()
{
    if (m_running) {
        return;
    }
    m_starting = true;
    m_status = {{QStringLiteral("phase"), QStringLiteral("preparing")}};
    m_running = true;
    Q_EMIT progressChanged();
    Schedule::startRun(QStringLiteral("manual"));
    // A run that finds nothing to do ends before it writes any progress.
    QTimer::singleShot(8000, this, [this] {
        if (m_starting) {
            m_starting = false;
            poll();
        }
    });
}

void BackupStatus::cancel()
{
    if (!m_running) {
        return;
    }
    if (isRunner(m_pid)) {
        ::kill(static_cast<pid_t>(m_pid), SIGTERM);
        m_stopping = true;
        Q_EMIT progressChanged();
    }
}

void BackupStatus::refreshWindows()
{
    QPointer<BackupStatus> self(this);
    QThread *thread = QThread::create([self] {
        const WindowsMode w = WindowsMode::query();
        // Through the application, which outlives this window's objects.
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, w] {
            if (!self) {
                return;
            }
            self->m_windowsPresent = w.present;
            self->m_windowsSetUp = w.setUp();
            self->m_windowsShutDown = w.shutDown();
            if (w.diskBytes >= 0) {
                self->m_windowsBytes = double(w.diskBytes);
            }
            Q_EMIT self->windowsChanged();
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
