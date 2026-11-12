#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// What Videos knows about folders: which files in one are videos, in the
// order Files shows them, and which folders under Videos have any. Read
// from the disk as asked; nothing is indexed.
class Library : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString videosFolder READ videosFolder CONSTANT)

public:
    explicit Library(QObject *parent);
    static Library *instance();
    static Library *create(QQmlEngine *, QJSEngine *);

    QString videosFolder() const;

    static bool isVideo(const QString &path);
    // The videos directly in a folder, by name as Files sorts them.
    Q_INVOKABLE QStringList videosIn(const QString &folder) const;
    // The folders directly in a folder that hold videos somewhere inside:
    // [{ path, name, count, cover }], cover being the first video.
    Q_INVOKABLE QVariantList foldersIn(const QString &folder) const;
    Q_INVOKABLE QString nameOf(const QString &path) const;
    Q_INVOKABLE QString parentOf(const QString &path) const;
    Q_INVOKABLE bool exists(const QString &path) const;
    // "Videos › Holidays" for a folder, as the overlay and the panel say it.
    Q_INVOKABLE QString placeOf(const QString &folder) const;
    // Opens Files at the item, selected.
    Q_INVOKABLE void showInFiles(const QString &path) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;

    // Tells QML when a watched folder's videos change, as a video is saved
    // or deleted beside the one playing.
    Q_INVOKABLE void watch(const QString &folder);

Q_SIGNALS:
    void folderChanged(const QString &folder);

private:
    QFileSystemWatcher m_watcher;
};
