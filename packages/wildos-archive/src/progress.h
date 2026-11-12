#pragma once

#include <QMutex>
#include <QString>

#include <atomic>

// Shared between a worker thread and the window that started it. The
// worker only writes; the window reads on a timer, so a busy archive never
// floods the event loop with progress signals.
struct Progress {
    std::atomic<bool> cancel{false};
    std::atomic<qint64> done{0};
    std::atomic<qint64> total{0};

    void setCurrent(const QString &name)
    {
        QMutexLocker lock(&mutex);
        current = name;
    }
    QString currentName()
    {
        QMutexLocker lock(&mutex);
        return current;
    }
    double fraction() const
    {
        const qint64 t = total.load();
        return t > 0 ? qBound(0.0, double(done.load()) / double(t), 1.0) : 0.0;
    }

private:
    QMutex mutex;
    QString current;
};
