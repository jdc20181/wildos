#pragma once

#include "progress.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <memory>

#include "session.h"

// The New archive dialog's work: the files and folders going in, the name,
// the format and the folder, and making the archive on a worker thread.
// The archive is written under a hidden name and only takes its own once
// it is complete, so a cancel leaves nothing behind.
class ArchiveMaker : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString totalText READ totalText NOTIFY itemsChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY itemsChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY nameChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QString folderText READ folderText NOTIFY folderChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY nameChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)

public:
    explicit ArchiveMaker(QObject *parent = nullptr);
    ~ArchiveMaker() override;

    QVariantList items() const;
    QString totalText() const;
    QString sourceName() const;
    QString name() const
    {
        return m_name;
    }
    void setName(const QString &name);
    QString format() const
    {
        return m_format;
    }
    void setFormat(const QString &format);
    QString folder() const
    {
        return m_folder;
    }
    QString folderText() const;
    QString problem() const;
    bool busy() const
    {
        return bool(m_progress);
    }
    double progress() const;
    QString progressText() const;

    // Starts afresh with these files, or none, saving beside the first.
    Q_INVOKABLE void reset(const QList<QUrl> &urls);
    // Starts afresh from an open archive's contents (Add files).
    Q_INVOKABLE void resetFrom(Archive *archive, const QList<QUrl> &urls);
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void setFolderUrl(const QUrl &url);
    Q_INVOKABLE QUrl folderUrl() const;
    Q_INVOKABLE void create();
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void itemsChanged();
    void nameChanged();
    void folderChanged();
    void busyChanged();
    void progressChanged();
    void done(bool ok, const QString &path, const QString &message, const QStringList &details);

private:
    struct Item {
        QString path;
        qint64 size = 0;
        bool dir = false;
    };
    QString suffix() const;
    QString targetPath() const;
    void suggestName();
    void stop();

    QList<Item> m_items;
    QString m_source;
    QByteArray m_sourcePassword;
    qint64 m_sourceSize = 0;
    QString m_name;
    QString m_format = QStringLiteral("zip");
    QString m_folder;
    bool m_named = false;

    std::shared_ptr<Progress> m_progress;
    QPointer<QThread> m_thread;
    QTimer m_ticker;
};
