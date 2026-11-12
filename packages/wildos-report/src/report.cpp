#include "report.h"
#include "parts.h"
#include "sysinfo.h"

#include <KZip>

#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrentRun>

namespace
{
const QString issuesUrl = QStringLiteral("https://github.com/wildorigins/wildos/issues");
// GitHub turns away longer addresses; under 8 KB is safe in every browser.
constexpr int urlBudget = 8000;

QString sizeName(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 bytes").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(qMax<qint64>(1, qRound64(bytes / 1024.0)));
    }
    return QStringLiteral("%1 MB").arg(QLocale(QLocale::English, QLocale::UnitedKingdom).toString(bytes / (1024.0 * 1024.0), 'f', 1));
}

// A log line as it reads in the saved file: "13:09:58  files: message".
QString fileLines(const QString &lines)
{
    QString out;
    out.reserve(lines.size() + lines.size() / 8);
    for (const QString &line : lines.split(QLatin1Char('\n'))) {
        const QStringList cols = line.split(QLatin1Char('\t'));
        out += cols.size() >= 3 ? cols.at(0) + QStringLiteral("  ") + cols.at(1) + QStringLiteral(": ") + cols.mid(2).join(QLatin1Char(' ')) : line;
        out += QLatin1Char('\n');
    }
    return out;
}

struct Redacted {
    QStringList texts;
    QList<QList<int>> counts;
};

Redacted redactTexts(const Redactor::Known &known, const QStringList &custom, const QStringList &texts)
{
    Redactor redactor;
    redactor.setKnown(known);
    redactor.setCustomWords(custom);
    redactor.reset();
    Redacted out;
    for (const QString &text : texts) {
        int before[Redactor::CategoryCount];
        for (int c = 0; c < Redactor::CategoryCount; ++c) {
            before[c] = redactor.count(Redactor::Category(c));
        }
        out.texts << redactor.redact(text);
        QList<int> counts;
        for (int c = 0; c < Redactor::CategoryCount; ++c) {
            counts << redactor.count(Redactor::Category(c)) - before[c];
        }
        out.counts << counts;
    }
    return out;
}
}

void PreviewModel::setLines(const QStringList &lines, bool log)
{
    beginResetModel();
    m_lines = lines;
    m_log = log;
    endResetModel();
    Q_EMIT countChanged();
}

int PreviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_lines.size());
}

QVariant PreviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size()) {
        return {};
    }
    const QString &line = m_lines.at(index.row());
    if (!m_log) {
        return role == TextRole ? QVariant(line) : role == LogRole ? QVariant(false) : QVariant(QString());
    }
    const qsizetype first = line.indexOf(QLatin1Char('\t'));
    const qsizetype second = first < 0 ? -1 : line.indexOf(QLatin1Char('\t'), first + 1);
    switch (role) {
    case TimeRole:
        return second < 0 ? QString() : line.left(first);
    case SourceRole:
        return second < 0 ? QString() : line.mid(first + 1, second - first - 1);
    case TextRole:
        return second < 0 ? line : line.mid(second + 1);
    case LogRole:
        return second >= 0;
    }
    return {};
}

QHash<int, QByteArray> PreviewModel::roleNames() const
{
    return {{TimeRole, "time"}, {SourceRole, "source"}, {TextRole, "text"}, {LogRole, "log"}};
}

Report::Report(QObject *parent)
    : QObject(parent)
    
{
    m_pickTime = QTime::currentTime().addSecs(-3600).toString(QStringLiteral("HH:mm"));
    const struct {
        const char *id;
        const char *file;
        bool included;
    } sections[] = {
        {"description", "report.md", true}, {"about", "about-this-pc.txt", true},     {"crash", "crash.txt", true},
        {"logs", "logs.txt", true},         {"updates", "recent-updates.txt", true}, {"fulllog", "full-system-log.txt", false},
    };
    for (const auto &s : sections) {
        Section section;
        section.id = QString::fromLatin1(s.id);
        section.file = QString::fromLatin1(s.file);
        section.included = s.included;
        m_sections << section;
    }
    connect(&m_collector, &Collector::finished, this, &Report::collected);
    m_capturePoll.setInterval(250);
    connect(&m_capturePoll, &QTimer::timeout, this, [this]() {
        const bool done = QFile::exists(m_capturePath);
        const bool cancelled = QFile::exists(m_capturePath + QStringLiteral(".cancelled"));
        // Capture can be closed without either, so give up after a while.
        const bool lost = m_captureStarted.secsTo(QDateTime::currentDateTime()) > 600;
        if (!done && !cancelled && !lost) {
            return;
        }
        m_capturePoll.stop();
        QFile::remove(m_capturePath + QStringLiteral(".cancelled"));
        if (done) {
            m_shots << m_capturePath;
            Q_EMIT screenshotsChanged();
            if (m_redactSerial > 0) {
                redactAll();
            }
        }
        m_capturing = false;
        Q_EMIT capturingChanged();
        Q_EMIT captureDone();
    });
}

Report *Report::instance()
{
    static Report *report = new Report;
    QQmlEngine::setObjectOwnership(report, QQmlEngine::CppOwnership);
    return report;
}

void Report::handle(const QStringList &arguments)
{
    QString part;
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &a = arguments.at(i);
        if (a.startsWith(QLatin1String("wildos-report:"))) {
            part = a.mid(14).section(QLatin1Char('?'), 0, 0).remove(QLatin1Char('/'));
        } else if (a == QLatin1String("--part") && i + 1 < arguments.size()) {
            part = arguments.at(++i);
        } else if (a == QLatin1String("--crash") && i + 1 < arguments.size()) {
            m_crashTime = QDateTime::fromMSecsSinceEpoch(arguments.at(++i).toLongLong() / 1000);
            Q_EMIT crashChanged();
        } else if (a == QLatin1String("--app") && i + 1 < arguments.size()) {
            m_crashApp = arguments.at(++i);
            Q_EMIT crashChanged();
        }
    }
    // A report already under way keeps its answers.
    if (!part.isEmpty() && Parts::known(part) && m_description.trimmed().isEmpty()) {
        setPart(part);
    }
    Q_EMIT raiseRequested();
}

QVariantList Report::parts() const
{
    return Parts::choices();
}

void Report::setPart(const QString &part)
{
    if (part == m_part || !Parts::known(part)) {
        return;
    }
    m_fixedSince = {};
    m_part = part;
    Q_EMIT partChanged();
}

QString Report::partName() const
{
    return Parts::byId(m_part).name;
}

void Report::setWhen(const QString &when)
{
    if (when != m_when) {
        m_fixedSince = {};
        m_when = when;
        Q_EMIT whenChanged();
    }
}

void Report::setPickDay(int day)
{
    if (day != m_pickDay) {
        m_fixedSince = {};
        m_pickDay = day;
        Q_EMIT whenChanged();
    }
}

void Report::setPickTime(const QString &time)
{
    if (time != m_pickTime && QTime::fromString(time, QStringLiteral("H:mm")).isValid()) {
        m_fixedSince = {};
        m_pickTime = time;
        Q_EMIT whenChanged();
    }
}

QPair<QDateTime, QDateTime> Report::window() const
{
    if (m_fixedSince.isValid()) {
        return {m_fixedSince, m_fixedUntil};
    }
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime since;
    QDateTime until = now;
    if (m_when == QLatin1String("today")) {
        since = QDateTime(now.date(), QTime(0, 0));
    } else if (m_when == QLatin1String("pick")) {
        const QDateTime at(now.date().addDays(-m_pickDay), QTime::fromString(m_pickTime, QStringLiteral("H:mm")));
        since = at.addSecs(-15 * 60);
        until = qMin(now, at.addSecs(15 * 60));
        if (since >= until) {
            since = until.addSecs(-30 * 60);
        }
    } else {
        // Ten minutes before the crash or now, so what led up to it is in.
        const bool recentCrash = m_crashTime.isValid() && m_crashTime.secsTo(now) < 6 * 3600;
        const QDateTime anchor = recentCrash ? m_crashTime : now;
        since = anchor.addSecs(-10 * 60);
        since.setTime(QTime(since.time().hour(), since.time().minute()));
        until = recentCrash ? qMin(now, anchor.addSecs(5 * 60)) : now;
    }
    return {since, until};
}

QString Report::windowText() const
{
    const auto [since, until] = window();
    QString text = QStringLiteral("%1 to %2").arg(since.toString(QStringLiteral("HH:mm")), until.toString(QStringLiteral("HH:mm")));
    if (until.date() != QDate::currentDate()) {
        text += QStringLiteral(" yesterday");
    }
    return text;
}

void Report::setDescription(const QString &text)
{
    if (text != m_description) {
        m_description = text;
        Q_EMIT descriptionChanged();
    }
}

void Report::setRepeat(const QString &repeat)
{
    if (repeat != m_repeat) {
        m_repeat = repeat;
        Q_EMIT repeatChanged();
    }
}

QString Report::repeatText() const
{
    return m_repeat == QLatin1String("always")      ? QStringLiteral("Every time")
        : m_repeat == QLatin1String("sometimes") ? QStringLiteral("Sometimes")
        : m_repeat == QLatin1String("once")      ? QStringLiteral("Only once")
                                                 : QString();
}

QVariantList Report::screenshots() const
{
    QVariantList list;
    for (const QString &path : m_shots) {
        const QFileInfo info(path);
        list.append(QVariantMap{
            {QStringLiteral("url"), QUrl::fromLocalFile(path)},
            {QStringLiteral("label"), QStringLiteral("Screenshot, %1").arg(info.lastModified().toString(QStringLiteral("HH:mm")))},
            {QStringLiteral("size"), sizeName(info.size())},
        });
    }
    return list;
}

QString Report::crashNote() const
{
    if (!m_crashTime.isValid()) {
        return {};
    }
    const QString app = m_crashApp.isEmpty() ? partName() : m_crashApp;
    return QStringLiteral("%1 closed unexpectedly at %2. Its details are added for you.").arg(app, m_crashTime.toString(QStringLiteral("HH:mm")));
}

void Report::takeScreenshot()
{
    if (m_capturing || !m_temp.isValid()) {
        return;
    }
    m_capturePath = m_temp.filePath(QStringLiteral("screenshot-%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    m_capturing = true;
    Q_EMIT capturingChanged();
    Q_EMIT hideForCapture();
    // Capture freezes the screen as it starts, so the window must be gone
    // by then, minimise animation and all.
    QTimer::singleShot(450, this, [this]() {
        m_captureStarted = QDateTime::currentDateTime();
        if (!QProcess::startDetached(QStringLiteral("wildos-capture"), {QStringLiteral("--to"), m_capturePath})) {
            QFile cancelled(m_capturePath + QStringLiteral(".cancelled"));
            if (cancelled.open(QIODevice::WriteOnly)) {
                cancelled.close();
            }
            Q_EMIT message(QStringLiteral("Capture couldn't be started"));
        }
        m_capturePoll.start();
    });
}

void Report::removeScreenshot(int index)
{
    if (index < 0 || index >= m_shots.size()) {
        return;
    }
    QFile::remove(m_shots.takeAt(index));
    Q_EMIT screenshotsChanged();
    if (m_redactSerial > 0) {
        redactAll();
    }
}

Report::Section *Report::section(const QString &id)
{
    for (Section &s : m_sections) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

const Report::Section *Report::section(const QString &id) const
{
    for (const Section &s : m_sections) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

// The person's own words and answers, as the report's first file.
QString Report::describe() const
{
    const auto [since, until] = window();
    QStringList lines;
    lines << QStringLiteral("# Problem report") << QString();
    lines << QStringLiteral("Part of Wild OS: %1").arg(partName());
    if (m_crashTime.isValid()) {
        lines << QStringLiteral("Closed unexpectedly at: %1").arg(m_crashTime.toString(QStringLiteral("d MMMM yyyy, HH:mm:ss")));
    }
    lines << QStringLiteral("When: %1 (%2 to %3)")
                 .arg(m_when == QLatin1String("today")  ? QStringLiteral("Earlier today")
                          : m_when == QLatin1String("pick") ? QStringLiteral("At a time picked")
                                                            : QStringLiteral("Just now"),
                      since.toString(QStringLiteral("d MMMM yyyy, HH:mm")), until.toString(QStringLiteral("HH:mm")));
    if (!repeatText().isEmpty()) {
        lines << QStringLiteral("Can it be made to happen again: %1").arg(repeatText());
    }
    lines << QString() << QStringLiteral("## What went wrong") << QString() << m_description.trimmed();
    if (!m_shots.isEmpty()) {
        lines << QString() << QStringLiteral("Screenshots: %1, in the screenshots folder.").arg(m_shots.size());
    }
    return lines.join(QLatin1Char('\n'));
}

void Report::prepare()
{
    m_fixedSince = {};
    m_fixedUntil = {};
    const auto [since, until] = window();
    // "Just now" moves on with the clock; the same part and choice of time
    // within two minutes is the same report.
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(m_part, m_when, m_pickTime).arg(m_pickDay) + QStringLiteral("|")
        + QString::number(since.toSecsSinceEpoch() / 120);
    if (key == m_collectedKey && !m_collecting) {
        m_fixedSince = m_raw.since;
        m_fixedUntil = m_raw.until;
        redactAll();
        return;
    }
    m_collectedKey = key;
    m_fixedSince = since;
    m_fixedUntil = until;
    Q_EMIT whenChanged();
    // The preview's title names the window too.
    Q_EMIT viewingChanged();
    m_collecting = true;
    Q_EMIT busyChanged();
    m_collector.start(since, until, Parts::byId(m_part), true);
}

void Report::collected(const RawReport &raw)
{
    m_raw = raw;
    m_collecting = false;
    // The serial numbers only root can read come with the logs.
    m_known = SysInfo::known(raw.serials);
    if (raw.crashTime.isValid() && !m_crashTime.isValid()) {
        m_crashTime = raw.crashTime;
        Q_EMIT crashChanged();
    }
    section(QStringLiteral("about"))->raw = raw.about;
    section(QStringLiteral("crash"))->raw = raw.crash;
    section(QStringLiteral("logs"))->raw = raw.logs.join(QLatin1Char('\n'));
    section(QStringLiteral("updates"))->raw = raw.updates;
    section(QStringLiteral("fulllog"))->raw = raw.boot.join(QLatin1Char('\n'));
    Q_EMIT busyChanged();
    Q_EMIT itemsChanged();
    redactAll();
}

void Report::redactAll()
{
    section(QStringLiteral("description"))->raw = describe();
    QStringList texts;
    for (const Section &s : std::as_const(m_sections)) {
        texts << s.raw;
    }
    const int serial = ++m_redactSerial;
    m_redacting = true;
    Q_EMIT busyChanged();
    auto *watcher = new QFutureWatcher<Redacted>(this);
    connect(watcher, &QFutureWatcher<Redacted>::finished, this, [this, watcher, serial]() {
        watcher->deleteLater();
        if (serial != m_redactSerial) {
            return;
        }
        const Redacted result = watcher->result();
        for (int i = 0; i < m_sections.size() && i < result.texts.size(); ++i) {
            m_sections[i].redacted = result.texts.at(i);
            for (int c = 0; c < Redactor::CategoryCount; ++c) {
                m_sections[i].counts[c] = result.counts.at(i).at(c);
            }
        }
        m_redacting = false;
        Q_EMIT busyChanged();
        Q_EMIT redactedChanged();
        Q_EMIT itemsChanged();
        showPreview();
        measure();
    });
    watcher->setFuture(QtConcurrent::run(redactTexts, m_known, m_custom, texts));
}

QVariantList Report::items() const
{
    QVariantList list;
    const auto add = [&list, this](const QString &id, const QString &title, const QString &subtitle, const QString &icon) {
        const Section *s = section(id);
        list.append(QVariantMap{{QStringLiteral("id"), id},
                                {QStringLiteral("title"), title},
                                {QStringLiteral("subtitle"), subtitle},
                                {QStringLiteral("icon"), icon},
                                {QStringLiteral("included"), s && s->included}});
    };
    const bool waiting = m_collecting;
    qint64 shotBytes = 0;
    for (const QString &path : m_shots) {
        shotBytes += QFileInfo(path).size();
    }
    const QString shots = m_shots.isEmpty() ? QStringLiteral("No screenshot")
        : m_shots.size() == 1                ? QStringLiteral("1 screenshot, %1").arg(sizeName(shotBytes))
                                             : QStringLiteral("%1 screenshots, %2").arg(m_shots.size()).arg(sizeName(shotBytes));
    add(QStringLiteral("description"), m_shots.size() > 1 ? QStringLiteral("Your description and screenshots")
                                       : m_shots.size() == 1 ? QStringLiteral("Your description and screenshot")
                                                             : QStringLiteral("Your description"),
        shots, QStringLiteral("file-text"));
    const QString v = SysInfo::version();
    add(QStringLiteral("about"), QStringLiteral("About this PC"),
        v.isEmpty() ? QStringLiteral("Wild OS version, the PC's model, graphics and memory")
                    : QStringLiteral("Wild OS %1, the PC's model, graphics and memory").arg(v),
        QStringLiteral("cpu"));
    if (!waiting && !m_raw.crash.isEmpty()) {
        const QString app = m_crashApp.isEmpty() ? partName() : m_crashApp;
        const QString at = m_raw.crashTime.isValid() ? m_raw.crashTime.toString(QStringLiteral("HH:mm")) : QString();
        add(QStringLiteral("crash"), QStringLiteral("Crash details for %1").arg(app),
            (m_raw.crashTrace ? QStringLiteral("Where it stopped, %1") : QStringLiteral("How it stopped, %1")).arg(at), QStringLiteral("zap"));
    }
    const Part &part = Parts::byId(m_part);
    QString logs;
    if (waiting) {
        logs = QStringLiteral("Gathering");
    } else {
        const int count = int(m_raw.logs.size());
        const QString lines = count == 1 ? QStringLiteral("1 line") : QStringLiteral("%1 lines").arg(count);
        if (!m_raw.systemLogs) {
            logs = part.programs.isEmpty() ? QStringLiteral("Your own apps only, %1").arg(lines) : QStringLiteral("%1 only, %2").arg(part.name, lines);
        } else {
            logs = part.programs.isEmpty() ? QStringLiteral("The system, %1").arg(lines) : QStringLiteral("%1 and the system, %2").arg(part.name, lines);
        }
    }
    add(QStringLiteral("logs"), QStringLiteral("Logs from %1").arg(windowText()), logs, QStringLiteral("logs"));
    add(QStringLiteral("updates"), QStringLiteral("Recent updates"),
        waiting || m_raw.updateDays > 0 ? QStringLiteral("What changed in the last 7 days") : QStringLiteral("Nothing changed in the last 7 days"),
        QStringLiteral("history"));
    const Section *full = section(QStringLiteral("fulllog"));
    add(QStringLiteral("fulllog"), QStringLiteral("Full system log"),
        waiting ? QStringLiteral("For problems that are hard to pin down")
                : QStringLiteral("For problems that are hard to pin down · %1").arg(sizeName(full->raw.toUtf8().size())),
        QStringLiteral("database"));
    return list;
}

void Report::setIncluded(const QString &id, bool on)
{
    Section *s = section(id);
    if (!s || s->included == on) {
        return;
    }
    s->included = on;
    Q_EMIT itemsChanged();
    Q_EMIT redactedChanged();
    measure();
}

void Report::view(const QString &id)
{
    if (id != m_viewing && section(id)) {
        m_viewing = id;
        Q_EMIT viewingChanged();
        showPreview();
    }
}

QString Report::previewTitle() const
{
    if (m_viewing == QLatin1String("description")) {
        return QStringLiteral("Preview: your description");
    }
    if (m_viewing == QLatin1String("about")) {
        return QStringLiteral("Preview: about this PC");
    }
    if (m_viewing == QLatin1String("crash")) {
        return QStringLiteral("Preview: crash details");
    }
    if (m_viewing == QLatin1String("updates")) {
        return QStringLiteral("Preview: recent updates");
    }
    if (m_viewing == QLatin1String("fulllog")) {
        return QStringLiteral("Preview: full system log");
    }
    return QStringLiteral("Preview: Logs from %1").arg(windowText());
}

void Report::showPreview()
{
    const Section *s = section(m_viewing);
    if (!s) {
        return;
    }
    const bool log = m_viewing == QLatin1String("logs") || m_viewing == QLatin1String("fulllog");
    QStringList lines = s->redacted.isEmpty() ? QStringList() : s->redacted.split(QLatin1Char('\n'));
    if (m_viewing == QLatin1String("description")) {
        for (int i = 0; i < m_shots.size(); ++i) {
            lines << QStringLiteral("screenshots/screenshot-%1.png, %2").arg(i + 1).arg(sizeName(QFileInfo(m_shots.at(i)).size()));
        }
    }
    if (lines.isEmpty()) {
        lines << (log ? QStringLiteral("Nothing was logged in this time.") : QStringLiteral("Nothing to show."));
    }
    m_preview.setLines(lines, log);
}

int Report::removed() const
{
    int total = 0;
    for (const Section &s : m_sections) {
        if (s.included) {
            for (int c = 0; c < Redactor::CategoryCount; ++c) {
                total += s.counts[c];
            }
        }
    }
    return total;
}

QVariantList Report::removedKinds() const
{
    QVariantList list;
    for (int c = 0; c < Redactor::CategoryCount; ++c) {
        int count = 0;
        for (const Section &s : m_sections) {
            if (s.included) {
                count += s.counts[c];
            }
        }
        if (count > 0) {
            list.append(QVariantMap{{QStringLiteral("label"), Redactor::categoryLabel(Redactor::Category(c))}, {QStringLiteral("count"), count}});
        }
    }
    return list;
}

void Report::addWord(const QString &word)
{
    const QString w = word.trimmed();
    if (w.size() < 2 || m_custom.contains(w, Qt::CaseInsensitive)) {
        return;
    }
    m_custom << w;
    Q_EMIT redactedChanged();
    redactAll();
}

void Report::removeWord(const QString &word)
{
    if (m_custom.removeAll(word) > 0) {
        Q_EMIT redactedChanged();
        redactAll();
    }
}

// The zip, built for real so the size shown is the size saved.
void Report::measure()
{
    const int serial = ++m_sizeSerial;
    auto *watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcher<qint64>::finished, this, [this, watcher, serial]() {
        watcher->deleteLater();
        if (serial == m_sizeSerial) {
            m_size = watcher->result();
            Q_EMIT sizeChanged();
        }
    });
    const QList<Section> sections = m_sections;
    const QStringList shots = m_shots;
    watcher->setFuture(QtConcurrent::run([sections, shots]() { return qint64(zip(sections, shots, false).size()); }));
}

QString Report::sizeText() const
{
    return m_size < 0 || busy() ? QString() : QStringLiteral("Report size %1").arg(sizeName(m_size));
}

QByteArray Report::zip(const QList<Section> &sections, const QStringList &shots, bool all)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    KZip archive(&buffer);
    if (!archive.open(QIODevice::WriteOnly)) {
        return {};
    }
    const QDateTime now = QDateTime::currentDateTime();
    for (const Section &s : sections) {
        if ((!s.included && !all) || s.redacted.isEmpty()) {
            continue;
        }
        QString text = s.redacted;
        if (s.id == QLatin1String("logs") || s.id == QLatin1String("fulllog")) {
            text = fileLines(text);
        }
        if (s.id == QLatin1String("description")) {
            int total = 0;
            for (const Section &other : sections) {
                if (other.included || all) {
                    for (int c = 0; c < Redactor::CategoryCount; ++c) {
                        total += other.counts[c];
                    }
                }
            }
            QStringList files;
            for (const Section &other : sections) {
                if ((other.included || all) && !other.redacted.isEmpty() && other.id != QLatin1String("description")) {
                    files << QStringLiteral("- ") + other.file;
                }
            }
            text += QStringLiteral("\n\n## In this report\n\n%1\n\nWritten with Report a problem in Wild OS. Personal details were replaced "
                                   "with placeholders such as [user] and [wifi-1] before anyone saw the report: %2 in all.\n")
                        .arg(files.join(QLatin1Char('\n')))
                        .arg(total);
        } else if (!text.endsWith(QLatin1Char('\n'))) {
            text += QLatin1Char('\n');
        }
        archive.writeFile(s.file, text.toUtf8(), 0100644, QString(), QString(), now, now, now);
    }
    const bool description = std::any_of(sections.begin(), sections.end(), [](const Section &s) {
        return s.id == QLatin1String("description") && s.included;
    });
    if (all || description) {
        for (int i = 0; i < shots.size(); ++i) {
            QFile shot(shots.at(i));
            if (shot.open(QIODevice::ReadOnly)) {
                archive.writeFile(QStringLiteral("screenshots/screenshot-%1.png").arg(i + 1), shot.readAll(), 0100644, QString(), QString(), now, now, now);
            }
        }
    }
    archive.close();
    return bytes;
}

QString Report::suggestedName() const
{
    return QStringLiteral("wildos-report-%1.zip").arg(QTime::currentTime().toString(QStringLiteral("HHmm")));
}

QUrl Report::downloadsFolder() const
{
    return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
}

QString Report::uniquePath(const QString &folder) const
{
    const QString base = suggestedName().chopped(4);
    QString path = QDir(folder).filePath(base + QStringLiteral(".zip"));
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = QDir(folder).filePath(QStringLiteral("%1-%2.zip").arg(base).arg(n));
    }
    return path;
}

QString Report::writeZip(const QString &path, bool all)
{
    const QByteArray bytes = zip(m_sections, m_shots, all);
    QFile file(path);
    if (bytes.isEmpty() || !file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) {
        return {};
    }
    file.close();
    m_savedPath = path;
    m_savedText = QStringLiteral("Report saved as %1, %2").arg(QFileInfo(path).fileName(), sizeName(bytes.size()));
    Q_EMIT savedChanged();
    return path;
}

void Report::setTitle(const QString &title)
{
    if (title != m_title) {
        m_title = title;
        m_titleTouched = true;
        Q_EMIT titleChanged();
    }
}

// The first sentence of the description, cut at a word, as a start.
void Report::suggestTitle()
{
    if (m_titleTouched && !m_title.trimmed().isEmpty()) {
        return;
    }
    const Section *s = section(QStringLiteral("description"));
    QString text = m_description.simplified();
    if (s && !s->redacted.isEmpty()) {
        const qsizetype at = s->redacted.indexOf(QStringLiteral("## What went wrong\n\n"));
        if (at >= 0) {
            text = s->redacted.mid(at + 20).section(QStringLiteral("\n\nScreenshots:"), 0, 0).simplified();
        }
    }
    const auto sentence = QRegularExpression(QStringLiteral("^(.+?[.!?])(\\s|$)")).match(text);
    if (sentence.hasMatch()) {
        text = sentence.captured(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text.size() > 80) {
        text = text.left(80);
        text = text.left(text.lastIndexOf(QLatin1Char(' '))) + QStringLiteral("...");
    }
    m_title = text;
    Q_EMIT titleChanged();
}

QString Report::issueBody(const QString &zipName, int budget) const
{
    const Section *s = section(QStringLiteral("description"));
    QString words = m_description.trimmed();
    if (s && !s->redacted.isEmpty()) {
        const qsizetype at = s->redacted.indexOf(QStringLiteral("## What went wrong\n\n"));
        if (at >= 0) {
            words = s->redacted.mid(at + 20).section(QStringLiteral("\n\nScreenshots:"), 0, 0).trimmed();
        }
    }
    if (budget >= 0 && words.size() > budget) {
        words = words.left(qMax(0, budget)).trimmed() + QStringLiteral("...\n\n(The rest is in the report.)");
    }
    const auto [since, until] = window();
    QStringList lines{words, QString(), QStringLiteral("| | |"), QStringLiteral("|---|---|")};
    lines << QStringLiteral("| Part of Wild OS | %1 |").arg(partName());
    lines << QStringLiteral("| When | %1 |").arg(m_crashTime.isValid() ? m_crashTime.toString(QStringLiteral("d MMMM yyyy, HH:mm"))
                                                                        : QStringLiteral("%1 to %2").arg(since.toString(QStringLiteral("d MMMM yyyy, HH:mm")),
                                                                                                         until.toString(QStringLiteral("HH:mm"))));
    if (!repeatText().isEmpty()) {
        lines << QStringLiteral("| Happens | %1 |").arg(repeatText());
    }
    const QString v = SysInfo::version();
    lines << QStringLiteral("| Wild OS | %1, %2 |").arg(v.isEmpty() ? QStringLiteral("unknown") : v, SysInfo::channel());
    const Section *about = section(QStringLiteral("about"));
    if (about && about->included && !SysInfo::model().isEmpty()) {
        lines << QStringLiteral("| PC | %1 |").arg(SysInfo::model());
    }
    if (!zipName.isEmpty()) {
        lines << QString() << QStringLiteral("**Logs and details:** drag `%1` from your Downloads folder into this box to attach it.").arg(zipName);
    }
    return lines.join(QLatin1Char('\n'));
}

bool Report::openGitHub()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(folder);
    const QString path = writeZip(uniquePath(folder), false);
    if (path.isEmpty()) {
        Q_EMIT message(QStringLiteral("The report couldn't be saved to Downloads"));
        return false;
    }
    const QString title = m_title.trimmed().isEmpty() ? QStringLiteral("Problem with %1").arg(partName()) : m_title.trimmed();
    const auto encode = [](const QString &value) { return QString::fromLatin1(QUrl::toPercentEncoding(value)); };
    const QString zipName = QFileInfo(path).fileName();
    const auto address = [&](int budget) {
        return issuesUrl + QStringLiteral("/new?title=") + encode(title) + QStringLiteral("&body=") + encode(issueBody(zipName, budget));
    };
    // A long description is cut to fit, and the whole of it is in the zip.
    QString url = address(-1);
    for (int budget = int(m_description.size()); url.size() > urlBudget && budget > 0; budget = budget * 4 / 5) {
        url = address(budget);
    }
    if (url.size() > urlBudget) {
        url = address(0);
    }
    QDesktopServices::openUrl(QUrl::fromEncoded(url.toLatin1()));
    return true;
}

bool Report::saveTo(const QUrl &file)
{
    QString path = file.toLocalFile();
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".zip");
    }
    if (writeZip(path, false).isEmpty()) {
        Q_EMIT message(QStringLiteral("The report couldn't be saved there"));
        return false;
    }
    return true;
}

void Report::copyDescription()
{
    QString text = issueBody(QString(), -1);
    QGuiApplication::clipboard()->setText(text);
    Q_EMIT message(QStringLiteral("Description copied"));
}

void Report::copyPreview()
{
    const Section *s = section(m_viewing);
    if (s) {
        const bool log = m_viewing == QLatin1String("logs") || m_viewing == QLatin1String("fulllog");
        QGuiApplication::clipboard()->setText(log ? fileLines(s->redacted) : s->redacted);
        Q_EMIT message(QStringLiteral("Copied"));
    }
}

void Report::searchIssues()
{
    const QString words = (m_title.trimmed().isEmpty() ? partName() : m_title).simplified();
    QDesktopServices::openUrl(QUrl(issuesUrl + QStringLiteral("?q=") + QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("is:issue ") + words))));
}

void Report::showSaved()
{
    if (m_savedPath.isEmpty()) {
        return;
    }
    // The file manager's own call, which opens the folder with the file
    // selected.
    QProcess::startDetached(QStringLiteral("dbus-send"),
                            {QStringLiteral("--session"), QStringLiteral("--type=method_call"), QStringLiteral("--dest=org.freedesktop.FileManager1"),
                             QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
                             QStringLiteral("array:string:") + QUrl::fromLocalFile(m_savedPath).toString(), QStringLiteral("string:")});
}

QString Report::saveHeadless(const QString &dir, const QString &part, const QDateTime &since, const QDateTime &until, int boot, const QString &description)
{
    if (Parts::known(part)) {
        m_part = part;
    }
    m_description = description.isEmpty() ? QStringLiteral("Saved from start-up recovery.") : description;
    m_fixedSince = since;
    m_fixedUntil = until;
    QEventLoop loop;
    RawReport raw;
    connect(&m_collector, &Collector::finished, &loop, [&](const RawReport &r) {
        raw = r;
        loop.quit();
    });
    disconnect(&m_collector, &Collector::finished, this, &Report::collected);
    m_collector.start(since, until, Parts::byId(m_part), false, boot);
    loop.exec();

    m_known = SysInfo::known(raw.serials);
    m_raw = raw;
    section(QStringLiteral("description"))->raw = describe().replace(QStringLiteral("When: Just now"), QStringLiteral("When: Saved from start-up recovery"));
    section(QStringLiteral("about"))->raw = raw.about;
    section(QStringLiteral("crash"))->raw = raw.crash;
    section(QStringLiteral("logs"))->raw = raw.logs.join(QLatin1Char('\n'));
    section(QStringLiteral("updates"))->raw = raw.updates;
    section(QStringLiteral("fulllog"))->raw = raw.boot.join(QLatin1Char('\n'));
    QStringList texts;
    for (const Section &s : std::as_const(m_sections)) {
        texts << s.raw;
    }
    const Redacted result = redactTexts(m_known, m_custom, texts);
    for (int i = 0; i < m_sections.size(); ++i) {
        m_sections[i].redacted = result.texts.at(i);
        for (int c = 0; c < Redactor::CategoryCount; ++c) {
            m_sections[i].counts[c] = result.counts.at(i).at(c);
        }
    }
    QDir().mkpath(dir);
    return writeZip(uniquePath(dir), true);
}
