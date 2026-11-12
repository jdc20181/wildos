#include "schedule.h"
#include "paths.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
const QString timer = QStringLiteral("wildos-backup.timer");
const QString watcher = QStringLiteral("wildos-backup-watch.service");

// systemctl calls one after another, in the background.
void runInTurn(QList<QStringList> commands)
{
    if (commands.isEmpty()) {
        return;
    }
    const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
    if (systemctl.isEmpty()) {
        return;
    }
    const QStringList args = commands.takeFirst();
    auto *process = new QProcess(QCoreApplication::instance());
    process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QObject::connect(process, &QProcess::finished, process, [process, commands]() {
        process->deleteLater();
        runInTurn(commands);
    });
    QObject::connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
        }
    });
    process->start(systemctl, QStringList{QStringLiteral("--user")} + args);
}
}

namespace Schedule
{
QString onCalendar(const QList<int> &days, int minutes)
{
    static const char *names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const QString at = QStringLiteral("%1:%2:00").arg(minutes / 60, 2, 10, QLatin1Char('0')).arg(minutes % 60, 2, 10, QLatin1Char('0'));
    if (days.size() >= 7) {
        return QStringLiteral("*-*-* ") + at;
    }
    QStringList list;
    for (int d : days) {
        list << QString::fromLatin1(names[d - 1]);
    }
    return list.join(QLatin1Char(',')) + QStringLiteral(" *-*-* ") + at;
}

void apply(const Settings *settings)
{
    const bool on = settings->configured() && settings->scheduled() && !settings->dayList().isEmpty();
    const QString path = Paths::timerDropIn();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        const QString body = QStringLiteral("# Set by Backup's Schedule page.\n[Timer]\nOnCalendar=\nOnCalendar=%1\n")
                                 .arg(onCalendar(settings->dayList().isEmpty() ? QList<int>{1, 2, 3, 4, 5, 6, 7} : settings->dayList(), settings->time()));
        f.write(body.toUtf8());
        f.commit();
    }

    QList<QStringList> commands{{QStringLiteral("daemon-reload")}};
    if (on) {
        commands.append({QStringLiteral("enable"), QStringLiteral("--now"), timer, watcher});
        // An already running timer keeps its old next time until restarted.
        commands.append({QStringLiteral("restart"), timer});
    } else if (settings->configured()) {
        // Still catch up when the drive comes back, but nothing on a timer.
        commands.append({QStringLiteral("disable"), QStringLiteral("--now"), timer});
        commands.append({QStringLiteral("enable"), QStringLiteral("--now"), watcher});
    } else {
        commands.append({QStringLiteral("disable"), QStringLiteral("--now"), timer, watcher});
    }
    runInTurn(commands);
}

void startRun(const QString &kind)
{
    const QString self = QCoreApplication::applicationFilePath();
    const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
    const QString unit = QStringLiteral("wildos-backup@%1.service").arg(kind);
    if (systemctl.isEmpty() || !QFileInfo::exists(QStringLiteral("/usr/lib/systemd/user/wildos-backup@.service"))) {
        QProcess::startDetached(self, {QStringLiteral("--run"), QStringLiteral("--") + kind});
        return;
    }
    auto *process = new QProcess(QCoreApplication::instance());
    QObject::connect(process, &QProcess::finished, process, [process, self, kind](int code) {
        process->deleteLater();
        // No user service manager (a container, an odd session): run it
        // straight from here instead.
        if (code != 0) {
            QProcess::startDetached(self, {QStringLiteral("--run"), QStringLiteral("--") + kind});
        }
    });
    QObject::connect(process, &QProcess::errorOccurred, process, [process, self, kind](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            QProcess::startDetached(self, {QStringLiteral("--run"), QStringLiteral("--") + kind});
        }
    });
    process->start(systemctl, {QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("--no-block"), unit});
}
}
