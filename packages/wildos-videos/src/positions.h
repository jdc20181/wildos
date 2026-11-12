#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

// Where you stopped in each video, kept on this PC only, in
// ~/.local/share/wildos-videos/positions.json. A video is "watched" once
// it has played past 95%, and then starts from the beginning next time.
// The same file gives the library its recent videos.
class Positions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Bumped on every change, so QML bindings that call progress() or
    // watched() read again.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    struct Entry {
        double position = 0;
        double duration = 0;
        bool watched = false;
        qint64 at = 0;
    };

    explicit Positions(QObject *parent);
    static Positions *instance();
    static Positions *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }

    // Where to start this file, in seconds: 0 for a new or watched one.
    double resumeAt(const QString &path) const;
    void remember(const QString &path, double position, double duration);
    void markWatched(const QString &path, double duration);

    Q_INVOKABLE double progress(const QString &path) const;
    Q_INVOKABLE bool watched(const QString &path) const;
    // From the right-click menu: marks a video watched, or forgets where
    // it was so it starts from the beginning.
    Q_INVOKABLE void setWatched(const QString &path, bool watched);
    // The most recently played videos that still exist, newest first.
    Q_INVOKABLE QStringList recent(int count) const;

    // Writes now, rather than after the short delay changes wait for.
    void save();

Q_SIGNALS:
    void changed();

private:
    void load();
    void touch();

    QHash<QString, Entry> m_entries;
    QTimer m_save;
    int m_revision = 0;
};
