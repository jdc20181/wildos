#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

// The Details tab of Properties: what the file says about itself (a
// photo's size and camera, a song's artist and album, a document's author
// and pages), read by KFileMetaData's extractors on a worker thread.
class FileMeta : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    // [{ label, value }], in the order the extractor reports them.
    Q_PROPERTY(QVariantList fields READ fields NOTIFY fieldsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY fieldsChanged)

public:
    explicit FileMeta(QObject *parent = nullptr);

    QUrl url() const { return m_url; }
    void setUrl(const QUrl &url);
    QVariantList fields() const { return m_fields; }
    bool loading() const { return m_loading; }

Q_SIGNALS:
    void urlChanged();
    void fieldsChanged();

private:
    QUrl m_url;
    QVariantList m_fields;
    bool m_loading = false;
    int m_generation = 0;
};
