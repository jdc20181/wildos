#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>

class KJob;

// What the Properties dialog shows for the selection:
// one item in full, several as a count and total size. Folder sizes are
// added up in the background.
class ItemInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<QUrl> urls READ urls WRITE setUrls NOTIFY urlsChanged)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString type READ type NOTIFY changed)
    Q_PROPERTY(QString location READ location NOTIFY changed)
    Q_PROPERTY(QString size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QString contains READ contains NOTIFY sizeChanged)
    Q_PROPERTY(bool counting READ counting NOTIFY sizeChanged)
    Q_PROPERTY(QString created READ created NOTIFY changed)
    Q_PROPERTY(QString modified READ modified NOTIFY changed)
    Q_PROPERTY(QString accessed READ accessed NOTIFY changed)
    Q_PROPERTY(QString opensWith READ opensWith NOTIFY changed)
    Q_PROPERTY(bool isDir READ isDir NOTIFY changed)
    Q_PROPERTY(bool single READ single NOTIFY changed)
    Q_PROPERTY(bool preview READ preview NOTIFY changed)
    Q_PROPERTY(qint64 stamp READ stamp NOTIFY changed)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly NOTIFY changed)
    Q_PROPERTY(bool canChangeReadOnly READ canChangeReadOnly NOTIFY changed)

public:
    explicit ItemInfo(QObject *parent = nullptr);
    ~ItemInfo() override;

    QList<QUrl> urls() const { return m_urls; }
    void setUrls(const QList<QUrl> &urls);

    QString name() const { return m_name; }
    QString fileName() const { return m_fileName; }
    QString iconName() const { return m_icon; }
    QString type() const { return m_type; }
    QString location() const { return m_location; }
    QString size() const { return m_size; }
    QString contains() const { return m_contains; }
    bool counting() const { return m_job != nullptr; }
    QString created() const { return m_created; }
    QString modified() const { return m_modified; }
    QString accessed() const { return m_accessed; }
    QString opensWith() const { return m_opensWith; }
    bool isDir() const { return m_isDir; }
    bool single() const { return m_urls.size() == 1; }
    bool preview() const { return m_preview; }
    qint64 stamp() const { return m_stamp; }
    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool on);
    bool canChangeReadOnly() const { return m_canChangeReadOnly; }

    Q_INVOKABLE void reload();

Q_SIGNALS:
    void urlsChanged();
    void changed();
    void sizeChanged();

private:
    void count(const QList<QUrl> &folders, qint64 filesBytes, int files);

    QList<QUrl> m_urls;
    QString m_name;
    QString m_fileName;
    QString m_icon;
    QString m_type;
    QString m_location;
    QString m_size;
    QString m_contains;
    QString m_created;
    QString m_modified;
    QString m_accessed;
    QString m_opensWith;
    bool m_isDir = false;
    bool m_preview = false;
    bool m_readOnly = false;
    bool m_canChangeReadOnly = false;
    qint64 m_stamp = 0;
    QPointer<KJob> m_job;
};
