#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QThreadPool>
#include <QVariantMap>

// A still from each video and its length, made on this PC by a headless
// libmpv that writes one frame a tenth of the way in as a JPEG. They are
// kept in ~/.cache/wildos-videos under a name made from the file's path,
// size and date, so a changed file gets a new one.
class Thumbnails : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Thumbnails(QObject *parent);
    ~Thumbnails() override;
    static Thumbnails *instance();
    static Thumbnails *create(QQmlEngine *, QJSEngine *);

    // { image: file URL or "", duration: seconds, width, height, ready }.
    // Asks for one in the background if it is not known yet; ready() says
    // when it is.
    Q_INVOKABLE QVariantMap info(const QString &path);

    // Called from the workers, on this object's thread.
    Q_INVOKABLE void finish(const QString &path, const QVariantMap &info);

Q_SIGNALS:
    void ready(const QString &path, const QVariantMap &info);

private:
    QHash<QString, QVariantMap> m_known;
    QSet<QString> m_pending;
    QThreadPool m_pool;
};
