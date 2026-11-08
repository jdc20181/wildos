#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

// What wildos-launchd remembers between sessions, in
// ~/.local/state/wildos/launch/:
//
//   usage.json  per desktop file: a launch score that fades over two weeks
//               and the files its process had mapped (for preloading)
//
// A small JSON file written a moment after a change, so a burst of
// launches costs one write.
class JsonFile : public QObject
{
    Q_OBJECT

public:
    explicit JsonFile(const QString &name, QObject *parent = nullptr);

    QJsonObject data() const { return m_data; }
    void setData(const QJsonObject &data);
    void flush();

    static QString directory();
    static QJsonObject read(const QString &name);

private:
    QString m_path;
    QJsonObject m_data;
    QTimer m_timer;
};

class UsageStore : public QObject
{
    Q_OBJECT

public:
    explicit UsageStore(QObject *parent = nullptr);

    void countLaunch(const QString &desktopId);
    void setFiles(const QString &desktopId, const QJsonArray &files);
    void flush() { m_file.flush(); }

    // The score with two weeks' fading applied up to now, so an app used
    // daily last month ranks below one used a few times this week.
    static double score(const QJsonObject &app, qint64 now);
    // Desktop ids, most used first.
    static QStringList ranked(const QJsonObject &apps);

private:
    JsonFile m_file;
};
