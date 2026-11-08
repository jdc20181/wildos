#pragma once

#include <QIcon>
#include <QQuickPaintedItem>
#include <QQmlEngine>
#include <QVariant>

// An icon from the icon theme (app icons, mime types) by name or path. The
// apps each registered their own image provider for this and the plasmoids
// used Kirigami.Icon; a painted item works in every one of those hosts.
//
// `source` takes what models hand over instead of a name: a QIcon (Plasma's
// app and file models), a picture, a path or URL. With `trim` on, the
// icon's own transparent margin is measured once and cut away, so icons
// drawn with generous padding and icons drawn edge to edge come out the
// same size; `contentScale` then leaves the share of the box the design's
// own icons leave (40 of 48 for the app icon grid).
class ThemeIcon : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString fallback READ fallback WRITE setFallback NOTIFY nameChanged)
    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY nameChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY nameChanged)
    Q_PROPERTY(bool trim READ trim WRITE setTrim NOTIFY trimChanged)
    Q_PROPERTY(qreal contentScale READ contentScale WRITE setContentScale NOTIFY trimChanged)

public:
    explicit ThemeIcon(QQuickItem *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name);
    QString fallback() const { return m_fallback; }
    void setFallback(const QString &fallback);
    QVariant source() const { return m_source; }
    void setSource(const QVariant &source);
    bool valid() const { return !m_icon.isNull(); }
    bool trim() const { return m_trim; }
    void setTrim(bool trim);
    qreal contentScale() const { return m_contentScale; }
    void setContentScale(qreal scale);

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void nameChanged();
    void trimChanged();

private:
    void resolve();
    QIcon iconFor(const QString &source) const;
    QRectF visibleBounds();

    QString m_name;
    QString m_fallback = QStringLiteral("application-x-executable");
    QVariant m_source;
    QIcon m_icon;
    QString m_key;
    bool m_trim = false;
    qreal m_contentScale = 1.0;
};
