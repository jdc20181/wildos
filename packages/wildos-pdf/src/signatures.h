#pragma once

#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QUrl>
#include <QVariant>

// The person's own signatures and initials, kept in the keyring (KWallet
// through QtKeychain) under "Wild OS PDF" and nowhere else: nothing is
// written to a file. A signature is drawn (its pen strokes), typed (the
// words, drawn in a handwriting font) or a picture (a PNG made from the
// file chosen, with the white around it made see-through).
//
// When the keyring can't be reached, signatures still work until PDF
// closes, and the Sign pane says they won't be kept.
class Signatures : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QString initials READ initials NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool kept READ kept NOTIFY readyChanged)

public:
    static Signatures *instance();
    static Signatures *create(QQmlEngine *, QJSEngine *);

    QVariantList items() const;
    QString initials() const;
    int revision() const;
    bool ready() const;
    bool kept() const;

    // Thread safe, for the image provider and for saving.
    QImage image(const QString &id) const;
    QImage initialsImage() const;

    Q_INVOKABLE void load();
    // strokes: [[Qt.point, ...], ...] in a box `aspect` times wider than
    // tall, each point 0 to 1 across and down it. Each returns the new id,
    // or "" if there was nothing to keep.
    Q_INVOKABLE QString addDrawn(const QVariantList &strokes, double aspect);
    Q_INVOKABLE QString addTyped(const QString &text);
    Q_INVOKABLE QString addImage(const QUrl &url);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void setInitials(const QString &initials);
    // Width over height of a signature's picture, for placing it.
    Q_INVOKABLE double aspect(const QString &id) const;
    Q_INVOKABLE double initialsAspect() const;

    static QImage renderTyped(const QString &text);

Q_SIGNALS:
    void changed();
    void readyChanged();

private:
    explicit Signatures(QObject *parent = nullptr);

    QImage renderEntry(const QJsonObject &entry) const;
    void rebuild();
    void store();
    QString add(QJsonObject entry);

    mutable QMutex m_lock;
    QJsonArray m_entries;
    QString m_initials;
    QHash<QString, QImage> m_images;
    QImage m_initialsImage;
    int m_revision = 0;
    bool m_ready = false;
    bool m_kept = true;
};

// image://signature/<id>/<revision> and image://signature/initials/<revision>
class SignatureProvider : public QQuickImageProvider
{
public:
    SignatureProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
