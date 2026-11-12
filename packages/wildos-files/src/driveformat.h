#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

// Formatting a removable drive, as Format... in File Explorer: unmount it,
// give it a file system through UDisks2 (polkit decides who may), then
// mount it again. A drive whose file system sits straight on the whole
// disk, or which has none, gets one partition first.
class DriveFormat : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // 0 to 1 while a full format writes over the drive, -1 when there is no
    // way to tell.
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    static DriveFormat *instance();
    static DriveFormat *create(QQmlEngine *, QJSEngine *);

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }

    // Whether Format... is offered: a removable or hot-plugged drive that
    // holds nothing of the running system. `udi` is Solid's, which for
    // UDisks2 drives is the block device's object path.
    Q_INVOKABLE bool canFormat(const QString &udi) const;
    // What the dialog shows: label, size, kind, readOnly, fatAllowed,
    // encryption and the file systems this PC has the tools for.
    Q_INVOKABLE QVariantMap describe(const QString &udi) const;
    // fs is "exfat", "ntfs", "vfat" or "ext4". An empty password means no
    // encryption.
    Q_INVOKABLE void start(const QString &udi, const QString &fs, const QString &label, bool quick, const QString &password);

    // /dev/sdb1, /dev/sdb or a UDisks2 object path to the object path, for
    // apps that ask Files to format a drive.
    static QString objectFor(const QString &device);

    struct Blank {
        QString udi; // the whole disk's UDisks2 object path
        QString kind; // "USB drive", "SD card", "external drive"
        qint64 size = 0;
    };
    // Removable drives with nothing on them Wild OS can read: no file
    // system, no encryption, not even a partition with something in it.
    // A new stick, or one a format left half done. Only drives Files may
    // format; a drive being formatted right now is left out.
    static QList<Blank> blankDrives();

Q_SIGNALS:
    void busyChanged();
    void progressChanged();
    // ok false with an empty message means the person cancelled at the
    // password prompt.
    void finished(bool ok, const QString &message, const QString &mountPath);

private:
    explicit DriveFormat(QObject *parent = nullptr);
    void poll();

    bool m_busy = false;
    double m_progress = -1;
    QString m_drive;
    QTimer m_poll;
};
