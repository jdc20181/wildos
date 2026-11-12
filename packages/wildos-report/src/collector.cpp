#include "collector.h"
#include "sysinfo.h"

#include <KCompressionDevice>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <unistd.h>

namespace
{
const QString helper = QStringLiteral("/usr/libexec/wildos/report-logs");

// The journal's MESSAGE is text, or an array of bytes when it isn't UTF-8.
QString message(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isArray()) {
        QByteArray bytes;
        for (const QJsonValue &b : value.toArray()) {
            bytes.append(char(b.toInt()));
        }
        return QString::fromUtf8(bytes);
    }
    return {};
}

struct Entry {
    qint64 usec = 0;
    QString line;
};

// One journal entry as a report line: "13:09:58<tab>files<tab>message".
// `keep` decides from the entry's account and program.
QList<Entry> parseJournal(const QByteArray &json, const std::function<bool(int uid, bool kernel, const QString &program)> &keep)
{
    QList<Entry> entries;
    for (const QByteArray &raw : json.split('\n')) {
        if (raw.trimmed().isEmpty()) {
            continue;
        }
        const QJsonObject o = QJsonDocument::fromJson(raw).object();
        if (o.isEmpty()) {
            continue;
        }
        const bool kernel = o.value(QStringLiteral("_TRANSPORT")).toString() == QLatin1String("kernel");
        const int uid = o.value(QStringLiteral("_UID")).toString().toInt();
        QString program = o.value(QStringLiteral("SYSLOG_IDENTIFIER")).toString();
        if (program.isEmpty()) {
            program = o.value(QStringLiteral("_COMM")).toString();
        }
        const QString comm = o.value(QStringLiteral("_COMM")).toString();
        if (!keep(uid, kernel, program) && !keep(uid, kernel, comm)) {
            continue;
        }
        const qint64 usec = o.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString().toLongLong();
        const QString time = QDateTime::fromMSecsSinceEpoch(usec / 1000).toString(QStringLiteral("HH:mm:ss"));
        QString source = kernel ? QStringLiteral("kernel") : program;
        if (source.startsWith(QLatin1String("wildos-"))) {
            source = source.mid(7);
        }
        QString text = message(o.value(QStringLiteral("MESSAGE")));
        text.replace(QLatin1Char('\t'), QLatin1Char(' '));
        const QStringList messageLines = text.split(QLatin1Char('\n'));
        for (const QString &part : messageLines) {
            if (!part.trimmed().isEmpty() || messageLines.size() == 1) {
                entries.append({usec, time + QLatin1Char('\t') + source + QLatin1Char('\t') + part});
            }
        }
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.usec < b.usec; });
    return entries;
}

QStringList lines(const QList<Entry> &entries)
{
    QStringList out;
    out.reserve(entries.size());
    for (const Entry &e : entries) {
        out << e.line;
    }
    return out;
}

QHash<QString, QByteArray> helperSections(const QByteArray &output)
{
    QHash<QString, QByteArray> sections;
    QString current;
    for (const QByteArray &line : output.split('\n')) {
        if (line.startsWith("@@ ")) {
            current = QString::fromUtf8(line.mid(3)).trimmed();
            sections.insert(current, {});
            continue;
        }
        if (!current.isEmpty()) {
            sections[current] += line + '\n';
        }
    }
    return sections;
}

// The last seven days of /var/log/apt/history.log (and the month before,
// rotated): when, and what was installed, updated or removed.
QString updates(int *days)
{
    const QDateTime from = QDateTime::currentDateTime().addDays(-7);
    QStringList blocks;
    QStringList files{QStringLiteral("/var/log/apt/history.log.1.gz"), QStringLiteral("/var/log/apt/history.log")};
    QString text;
    for (const QString &path : files) {
        if (!QFile::exists(path)) {
            continue;
        }
        if (path.endsWith(QLatin1String(".gz"))) {
            KCompressionDevice device(path, KCompressionDevice::GZip);
            if (device.open(QIODevice::ReadOnly)) {
                text += QString::fromUtf8(device.readAll());
            }
        } else {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                text += QString::fromUtf8(file.readAll());
            }
        }
        text += QLatin1Char('\n');
    }
    static const QRegularExpression packageItem(QStringLiteral("([^\\s,:]+)(?::[a-z0-9]+)? \\(([^)]*)\\)"));
    QSet<QDate> seen;
    for (const QString &block : text.split(QStringLiteral("\n\n"))) {
        QString start;
        QStringList body;
        for (const QString &line : block.split(QLatin1Char('\n'))) {
            const int colon = line.indexOf(QStringLiteral(": "));
            if (colon < 0) {
                continue;
            }
            const QString key = line.left(colon);
            const QString value = line.mid(colon + 2);
            if (key == QLatin1String("Start-Date")) {
                start = value.simplified();
                continue;
            }
            const QString verb = key == QLatin1String("Install")     ? QStringLiteral("Installed")
                : key == QLatin1String("Upgrade")                    ? QStringLiteral("Updated")
                : key == QLatin1String("Remove") || key == QLatin1String("Purge") ? QStringLiteral("Removed")
                : key == QLatin1String("Downgrade")                  ? QStringLiteral("Downgraded")
                : key == QLatin1String("Reinstall")                  ? QStringLiteral("Reinstalled")
                                                                     : QString();
            // The command line and who asked are left out: neither helps,
            // and the second names the account.
            if (verb.isEmpty()) {
                continue;
            }
            for (auto it = packageItem.globalMatch(value); it.hasNext();) {
                const auto m = it.next();
                QString versions = m.captured(2).remove(QStringLiteral(", automatic"));
                versions.replace(QStringLiteral(", "), QStringLiteral(" to "));
                body << QStringLiteral("  %1 %2 %3").arg(verb, m.captured(1), versions);
            }
        }
        const QDateTime when = QDateTime::fromString(start, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!when.isValid() || when < from || body.isEmpty()) {
            continue;
        }
        seen.insert(when.date());
        blocks << when.toString(QStringLiteral("d MMMM yyyy, HH:mm")) + QLatin1Char('\n') + body.join(QLatin1Char('\n'));
    }
    *days = seen.size();
    return blocks.isEmpty() ? QStringLiteral("Nothing was installed, updated or removed in the last 7 days.") : blocks.join(QStringLiteral("\n\n"));
}

// coredumpctl info prints one record per crash, each starting at its PID
// line; keep those of the part's programs.
QString crashRecords(const QString &text, const Part &part, QString *program, QDateTime *time, bool *trace)
{
    QStringList records;
    QString current;
    static const QRegularExpression pidLine(QStringLiteral("^\\s*PID: \\d+ \\(([^)]*)\\)"));
    const auto flush = [&]() {
        if (current.trimmed().isEmpty()) {
            return;
        }
        const auto exe = QRegularExpression(QStringLiteral("^\\s*Executable: (\\S+)"), QRegularExpression::MultilineOption).match(current);
        const auto comm = pidLine.match(current);
        const QString name = exe.hasMatch() ? QFileInfo(exe.captured(1)).fileName() : comm.captured(1);
        if (part.programs.isEmpty() || part.programs.contains(name) || part.programs.contains(comm.captured(1))) {
            records << current.trimmed();
            *program = name;
            const auto stamp = QRegularExpression(QStringLiteral("^\\s*Timestamp: \\S+ (\\S+ \\S+)"), QRegularExpression::MultilineOption).match(current);
            if (stamp.hasMatch()) {
                *time = QDateTime::fromString(stamp.captured(1), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            }
            *trace = *trace || current.contains(QLatin1String("Stack trace of thread"));
        }
        current.clear();
    };
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (pidLine.match(line).hasMatch()) {
            flush();
        }
        current += line + QLatin1Char('\n');
    }
    flush();
    return records.join(QStringLiteral("\n\n"));
}

bool isSystem(int uid, bool kernel)
{
    return kernel || uid < 1000 || (uid >= 61184 && uid <= 65519);
}
}

Collector::Collector(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<RawReport>();
}

Collector::~Collector()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void Collector::start(const QDateTime &since, const QDateTime &until, const Part &part, bool screens, int boot)
{
    ++m_serial;
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->deleteLater();
    }
    m_since = since;
    m_until = until;
    m_part = part;
    m_screens = screens;
    m_boot = boot;

    auto *process = new QProcess(this);
    m_process = process;
    const QStringList arguments{QStringLiteral("collect"), QString::number(since.toSecsSinceEpoch()), QString::number(until.toSecsSinceEpoch()),
                                QString::number(boot)};
    const int serial = m_serial;
    connect(process, &QProcess::finished, this, [this, process, serial](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (serial != m_serial) {
            return;
        }
        if (status == QProcess::NormalExit && code == 0) {
            finish(process->readAllStandardOutput(), {}, {});
        } else {
            // Not an administrator and no password given, or no helper:
            // this account's own log is all there is.
            readOwnJournal();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, serial](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && serial == m_serial) {
            process->deleteLater();
            readOwnJournal();
        }
    });
    if (geteuid() == 0) {
        process->start(helper, arguments);
    } else {
        process->start(QStringLiteral("pkexec"), QStringList{helper} + arguments);
    }
}

void Collector::readOwnJournal()
{
    const int serial = m_serial;
    const QString fields = QStringLiteral("--output-fields=__REALTIME_TIMESTAMP,_TRANSPORT,SYSLOG_IDENTIFIER,_COMM,MESSAGE,PRIORITY,_UID");
    auto *window = new QProcess(this);
    m_process = window;
    connect(window, &QProcess::finished, this, [this, window, serial, fields]() {
        window->deleteLater();
        if (serial != m_serial) {
            return;
        }
        const QByteArray own = window->readAllStandardOutput();
        auto *boot = new QProcess(this);
        m_process = boot;
        connect(boot, &QProcess::finished, this, [this, boot, own, serial]() {
            boot->deleteLater();
            if (serial == m_serial) {
                finish({}, own, boot->readAllStandardOutput());
            }
        });
        boot->start(QStringLiteral("journalctl"), {QStringLiteral("--user"), QStringLiteral("--no-pager"), QStringLiteral("-q"), QStringLiteral("-o"),
                                                   QStringLiteral("json"), fields, QStringLiteral("-b"), QString::number(m_boot), QStringLiteral("-n"),
                                                   QStringLiteral("60000")});
    });
    window->start(QStringLiteral("journalctl"),
                  {QStringLiteral("--user"), QStringLiteral("--no-pager"), QStringLiteral("-q"), QStringLiteral("-o"), QStringLiteral("json"), fields,
                   QStringLiteral("--since"), QStringLiteral("@%1").arg(m_since.toSecsSinceEpoch()), QStringLiteral("--until"),
                   QStringLiteral("@%1").arg(m_until.toSecsSinceEpoch()), QStringLiteral("-n"), QStringLiteral("20000")});
}

void Collector::finish(const QByteArray &helperOutput, const QByteArray &ownLogs, const QByteArray &ownBoot)
{
    m_process.clear();
    const int serial = m_serial;
    const Part part = m_part;
    const QDateTime since = m_since;
    const QDateTime until = m_until;
    const bool screens = m_screens;
    const int self = int(getuid());

    // About this PC asks QScreen, which belongs to this thread.
    const QString about = SysInfo::aboutPc(part.package, screens);

    auto *watcher = new QFutureWatcher<RawReport>(this);
    connect(watcher, &QFutureWatcher<RawReport>::finished, this, [this, watcher, serial]() {
        watcher->deleteLater();
        if (serial == m_serial) {
            Q_EMIT finished(watcher->result());
        }
    });
    watcher->setFuture(QtConcurrent::run([=]() {
        RawReport report;
        report.since = since;
        report.until = until;
        report.about = about;
        // The system's lines, and the part's own; the whole log has all of
        // this account's.
        const auto inWindow = [&](int uid, bool kernel, const QString &program) {
            if (isSystem(uid, kernel)) {
                return true;
            }
            if (uid != self && self != 0) {
                return false;
            }
            return part.programs.isEmpty() || part.programs.contains(program);
        };
        const auto inBoot = [&](int uid, bool kernel, const QString &) { return isSystem(uid, kernel) || uid == self || self == 0; };
        if (!helperOutput.isEmpty()) {
            const auto sections = helperSections(helperOutput);
            report.systemLogs = true;
            report.logs = lines(parseJournal(sections.value(QStringLiteral("logs")), inWindow));
            report.boot = lines(parseJournal(sections.value(QStringLiteral("boot")), inBoot));
            report.crash = crashRecords(QString::fromUtf8(sections.value(QStringLiteral("crashes"))), part, &report.crashProgram, &report.crashTime,
                                        &report.crashTrace);
            for (const QString &line : QString::fromUtf8(sections.value(QStringLiteral("ids"))).split(QLatin1Char('\n'))) {
                const QString value = line.section(QLatin1Char('='), 1).trimmed();
                if (value.size() >= 4) {
                    report.serials << value << QString(value).remove(QLatin1Char('-'));
                }
            }
        } else {
            report.logs = lines(parseJournal(ownLogs, inWindow));
            report.boot = lines(parseJournal(ownBoot, inBoot));
        }
        if (report.crash.isEmpty()) {
            report.crash = crashNotes(since, until, part, &report.crashProgram, &report.crashTime);
        }
        report.updates = updates(&report.updateDays);
        return report;
    }));
}

QString Collector::crashNotes(const QDateTime &since, const QDateTime &until, const Part &part, QString *program, QDateTime *time)
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        return {};
    }
    QStringList records;
    for (const QString &folder : {QStringLiteral("seen"), QStringLiteral("crashes")}) {
        const QDir dir(runtime + QStringLiteral("/wildos-report/") + folder);
        for (const QString &name : dir.entryList({QStringLiteral("*.crash")}, QDir::Files, QDir::Name)) {
            QFile file(dir.filePath(name));
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            QHash<QString, QString> note;
            for (const QString &line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
                note.insert(line.section(QLatin1Char('='), 0, 0), line.section(QLatin1Char('='), 1));
            }
            const QDateTime when = QDateTime::fromMSecsSinceEpoch(note.value(QStringLiteral("time")).toLongLong() / 1000);
            const QString exe = QFileInfo(note.value(QStringLiteral("exe"))).fileName();
            const QString comm = note.value(QStringLiteral("comm"));
            if (when < since.addSecs(-60) || when > until.addSecs(60)) {
                continue;
            }
            if (!part.programs.isEmpty() && !part.programs.contains(exe) && !part.programs.contains(comm)) {
                continue;
            }
            *program = exe.isEmpty() ? comm : exe;
            *time = when;
            records << QStringLiteral("Program: %1 (%2)\nSignal: %3\nTime: %4")
                           .arg(comm, note.value(QStringLiteral("exe")), note.value(QStringLiteral("signal")),
                                when.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        }
    }
    return records.join(QStringLiteral("\n\n"));
}
