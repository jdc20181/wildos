#pragma once

#include <QObject>
#include <QQmlEngine>

class QFileSystemWatcher;

// "Use every processor thread" (hyper-threading), in System > Power and
// battery, with shortcuts from Windows mode. Wild OS starts with the second
// thread of each core off; turning it on is a boot option written by
// settings-cpu-threads, so what was chosen and what the running kernel does
// can differ until the next start-up. Strict and Maximum protection keep it off.
class Processor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Whether the processor has two threads per core at all.
    Q_PROPERTY(bool threadsAvailable READ threadsAvailable CONSTANT)
    // What was chosen; it applies from the next start-up.
    Q_PROPERTY(bool everyThread READ everyThread NOTIFY changed)
    // What the running kernel does.
    Q_PROPERTY(bool everyThreadActive READ everyThreadActive NOTIFY changed)
    // Strict or Maximum protection, which keep it off.
    Q_PROPERTY(bool everyThreadLocked READ everyThreadLocked NOTIFY changed)
    // A change is waiting for the password or being written.
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    explicit Processor(QObject *parent = nullptr);

    bool threadsAvailable() const;
    bool everyThread() const;
    bool everyThreadActive() const;
    bool everyThreadLocked() const;
    bool busy() const { return m_busy; }

    Q_INVOKABLE void setEveryThread(bool on);

Q_SIGNALS:
    void changed();
    void failed(const QString &message);

private:
    QFileSystemWatcher *m_watcher = nullptr;
    bool m_busy = false;
};
