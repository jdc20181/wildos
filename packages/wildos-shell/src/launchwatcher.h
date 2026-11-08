#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

class QDBusServiceWatcher;
class QTimer;

// The apps wildos-launchd is opening, for the taskbar's starting line.
//
// KWin's launch feedback ends as soon as any other window is activated,
// while a slow app may still be on its way. The launch daemon keeps waiting
// for the app's window and says so on D-Bus (org.wildos.Launch:
// LaunchStarted, LaunchWaiting, LaunchEnded), so the taskbar follows these
// as well.
// QML in plasmashell has no way to listen to a D-Bus signal, hence this.
//
// A launch the daemon does not wait for (the app already had a window)
// gets LaunchStarted but no LaunchWaiting or LaunchEnded; it is dropped
// after a moment. Every launch is dropped after a minute at
// most, and all of them when the daemon goes away.
class LaunchWatcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // [{ desktopId, name, icon, confirmed }], oldest first. Confirmed: the
    // daemon is known to be waiting for it (LaunchWaiting, or its State
    // listed it).
    Q_PROPERTY(QVariantList launches READ launches NOTIFY launchesChanged)

public:
    explicit LaunchWatcher(QObject *parent = nullptr);

    QVariantList launches() const;

    // True while the daemon is opening the app with this desktop id (with
    // or without ".desktop"), or with this launcher URL.
    Q_INVOKABLE bool isStarting(const QString &key) const;

Q_SIGNALS:
    void launchesChanged();

private Q_SLOTS:
    void started(const QString &desktopId);
    void waiting(const QString &desktopId);
    void ended(const QString &desktopId);

private:
    struct Entry {
        QString name;
        QString icon;
        bool confirmed = false;
        qint64 since = 0;
        qint64 order = 0;
    };

    void add(const QString &desktopId, bool confirmed);
    void expire();
    void scheduleExpiry();
    void loadState();
    static QString normalise(const QString &key);

    QHash<QString, Entry> m_entries;
    QDBusServiceWatcher *m_watcher = nullptr;
    QTimer *m_expiry = nullptr;
    qint64 m_next = 0;
};
