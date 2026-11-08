#pragma once

#include <QPixmap>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QQmlEngine>
#include <QUrl>

namespace KIO
{
class PreviewJob;
}

// A picture or video's own image in place of its icon, made by KIO's
// thumbnailers and cached by them. `ready` stays false until there is one,
// so the icon shows meanwhile and for files with nothing to show.
class Thumbnail : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(qint64 stamp READ stamp WRITE setStamp NOTIFY urlChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit Thumbnail(QQuickItem *parent = nullptr);
    ~Thumbnail() override;

    QUrl url() const { return m_url; }
    void setUrl(const QUrl &url);
    qint64 stamp() const { return m_stamp; }
    void setStamp(qint64 stamp);
    bool ready() const { return !m_pixmap.isNull(); }

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void urlChanged();
    void readyChanged();

protected:
    void componentComplete() override;

private:
    void load();

    QUrl m_url;
    qint64 m_stamp = 0;
    QPixmap m_pixmap;
    QPointer<KIO::PreviewJob> m_job;
};
