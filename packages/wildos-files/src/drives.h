#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct Drive {
    QString udi;
    QString name;
    QString themeIcon;
    QString lineIcon;
    QString mountPath;
    // What kind of drive it is, as a chip on its card: "Encrypted" for a
    // Wild OS disk or a USB stick behind LUKS, "Windows" for NTFS,
    // "Removable".
    QString chip;
    bool mounted = false;
    bool removable = false;
    bool system = false;
    // Plugged in but with nothing readable on it: it can only be formatted.
    bool blank = false;
    qint64 total = 0;
    qint64 free = -1;
};

// The drives a person would recognise, labelled as ThisComputer.dc.html
// does: the system disk is "Wild OS", the Windows system partition is
// "Windows (C:)", others go by their label or size.
// Boot, swap, loop and other plumbing mounts never appear.
class Drives : public QObject
{
    Q_OBJECT

public:
    static Drives *instance();

    QList<Drive> list() const;
    // The drive a local path lives on, for breadcrumbs and properties.
    Drive driveFor(const QString &path) const;
    void mount(const QString &udi);
    void eject(const QString &udi);
    // Free space changes without any device event; this reads it again.
    void refresh() { m_dirty = true; }

Q_SIGNALS:
    void changed();
    void mounted(const QString &udi, const QString &path);
    void failed(const QString &message);
    // Opening a blank drive: Files offers to format it, as Windows does.
    void needsFormat(const QString &udi, const QString &name);

private:
    explicit Drives(QObject *parent = nullptr);
    void watch(const QString &udi);
    QList<Drive> scan() const;

    mutable QList<Drive> m_cache;
    mutable bool m_dirty = true;
};
