#pragma once

#include "parts.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>

// Everything a report can hold, before personal details are removed.
struct RawReport {
    // The window it was gathered for.
    QDateTime since;
    QDateTime until;
    QString about;
    // coredumpctl's record of each crash of the part in the window.
    QString crash;
    QString crashProgram;
    QDateTime crashTime;
    bool crashTrace = false;
    // "HH:MM:SS<tab>source<tab>message", oldest first.
    QStringList logs;
    QStringList boot;
    QString updates;
    int updateDays = 0;
    // Serial numbers only root can read, for the redactor.
    QStringList serials;
    // False when the system's own log could not be read (no administrator):
    // the logs are then this account's only.
    bool systemLogs = false;
};

// Reads the logs, crash records, updates and the PC's details for a time
// window. The system log comes through report-logs, under pkexec unless
// this already runs as root (start-up recovery); without it, only this
// account's own log can be read.
class Collector : public QObject
{
    Q_OBJECT

public:
    explicit Collector(QObject *parent = nullptr);
    ~Collector() override;

    // `boot` 0 is this boot, -1 the one before.
    void start(const QDateTime &since, const QDateTime &until, const Part &part, bool screens, int boot = 0);
    bool running() const { return !m_process.isNull(); }

    // The crash notes report-crash-note left for this account, as text, for
    // when the system log can't be read.
    static QString crashNotes(const QDateTime &since, const QDateTime &until, const Part &part, QString *program, QDateTime *time);

Q_SIGNALS:
    void finished(const RawReport &report);

private:
    void readOwnJournal();
    void finish(const QByteArray &helperOutput, const QByteArray &ownLogs, const QByteArray &ownBoot);

    QPointer<QProcess> m_process;
    QDateTime m_since;
    QDateTime m_until;
    Part m_part;
    bool m_screens = true;
    int m_boot = 0;
    int m_serial = 0;
};

Q_DECLARE_METATYPE(RawReport)
