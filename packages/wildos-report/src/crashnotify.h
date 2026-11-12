#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

// wildos-report --crash-notify, started by wildos-report-crash.path when
// report-crash-note leaves a note: "<App> closed unexpectedly", with Not
// now and Report a problem, at most once per app per day. Which apps were
// offered today is kept in ~/.cache/wildos-report. It stays while its
// notifications can still be answered, then ends.
class CrashNotify : public QObject
{
    Q_OBJECT

public:
    explicit CrashNotify(QObject *parent = nullptr);
    void start();

private:
    void read();
    void notify(const QString &app, const QString &part, const QString &time);
    void idle();

    QString m_dir;
    QString m_seen;
    QFileSystemWatcher m_watcher;
    QTimer m_quit;
    int m_open = 0;
};
