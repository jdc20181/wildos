#include "processor.h"

#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>

namespace
{
const QString wildosDir = QStringLiteral("/etc/wildos");
const QString choiceFile = QStringLiteral("/etc/wildos/cpu-threads");
const QString helper = QStringLiteral("/usr/libexec/wildos/settings-cpu-threads");

QByteArray readSmt(const char *name)
{
    QFile file(QStringLiteral("/sys/devices/system/cpu/smt/") + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
}
}

Processor::Processor(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    // The choice and the protection level both live in /etc/wildos, written by
    // root helpers, so the switch follows them without Settings asking.
    if (QFile::exists(wildosDir)) {
        m_watcher->addPath(wildosDir);
    }
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &Processor::changed);
}

// "notsupported" and "notimplemented" mean a processor with one thread per core.
bool Processor::threadsAvailable() const
{
    const QByteArray control = readSmt("control");
    return QFile::exists(helper) && !control.isEmpty() && control != "notsupported" && control != "notimplemented";
}

bool Processor::everyThread() const
{
    return QFile::exists(choiceFile);
}

bool Processor::everyThreadActive() const
{
    return readSmt("active") == "1";
}

bool Processor::everyThreadLocked() const
{
    QFile file(QStringLiteral("/etc/wildos/protection-level"));
    const QByteArray level = file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
    return level == "strict" || level == "maximum";
}

void Processor::setEveryThread(bool on)
{
    if (m_busy || on == everyThread() || (on && everyThreadLocked())) {
        return;
    }
    m_busy = true;
    Q_EMIT changed();
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int code) {
        process->deleteLater();
        m_busy = false;
        if (code == 126 || code == 127) {
            Q_EMIT failed(QStringLiteral("Nothing was changed because the password was not given."));
        } else if (code == 3) {
            Q_EMIT failed(QStringLiteral("Strict and Maximum protection keep this off."));
        } else if (code != 0) {
            Q_EMIT failed(QStringLiteral("The setting could not be changed."));
        }
        Q_EMIT changed();
    });
    process->start(QStringLiteral("pkexec"), {helper, on ? QStringLiteral("on") : QStringLiteral("off")});
}
