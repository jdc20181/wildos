#pragma once

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

#include <functional>

#include "driveclass.h"

class Sizes;

// The drives plugged in now, over UDisks2 on the system bus, and mounting
// one. The app watches for drives coming and going; the scheduled run asks
// once.
class Drives : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Usable drives plugged in now: [{ uuid, name, kind, size, free, mountPoint, known }]
    Q_PROPERTY(QVariantList available READ available NOTIFY changed)
    // Every drive the person plugged in, usable or not, each with its
    // reason and whether formatting would fix it.
    Q_PROPERTY(QVariantList all READ all NOTIFY changed)
    // The person's own drives by filesystem UUID, plugged in or not, as
    // info() gives them; and which of them a backup would use now. Real
    // properties, so bindings follow drives coming, going and mounting.
    Q_PROPERTY(QVariantMap mine READ mine NOTIFY changed)
    Q_PROPERTY(QString pluggedIn READ pluggedIn NOTIFY changed)

public:
    explicit Drives(QObject *parent);
    static Drives *instance();
    static Drives *create(QQmlEngine *, QJSEngine *);

    // Asks UDisks2 now; blocking, a few milliseconds.
    static QList<DriveInfo> scan();
    static DriveInfo find(const QString &uuid);
    // Mounts it if it is not, and gives where. `interactive` lets polkit
    // ask for a password; the scheduled run never does.
    static QString mount(const DriveInfo &drive, bool interactive, QString *error);
    // mount() off the main thread, the answer back on it while `owner` lives.
    static void mountLater(QObject *owner, const QString &uuid,
                           std::function<void(const QString &mountPoint, const QString &error, const DriveInfo &info)> done);
    // The person's drive to read backups from: the default if plugged in,
    // else any other of theirs that is; empty if none.
    QString pluggedIn() const;
    QVariantMap mine() const;
    static qint64 freeBytes(const QString &mountPoint);
    static qint64 totalBytes(const QString &mountPoint);

    QVariantList available() const;
    QVariantList all() const;
    // About how much the parts picked in What's included hold; 0 until
    // that is worked out.
    qint64 needed() const;

    // The one place Backup hands a drive to Files to format.
    Q_INVOKABLE void formatDrive(const QString &device);

    // For the pages: is it plugged in, how full, what is it called now.
    Q_INVOKABLE QVariantMap info(const QString &uuid) const;
    Q_INVOKABLE void refresh();
    // Starts watching UDisks2; the app calls this, the scheduled run does not.
    void watch();

Q_SIGNALS:
    void changed();
    // A drive with this filesystem appeared (for the watcher).
    void appeared(const QString &uuid);

private Q_SLOTS:
    void somethingChanged();

private:
    QList<DriveInfo> m_drives;
    Sizes *m_sizes = nullptr;
    QTimer m_debounce;
    bool m_watching = false;
};
