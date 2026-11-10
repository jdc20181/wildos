#pragma once

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The message in the reading pane: who sent it, its body ready for
// HtmlView, its attachments and what was kept out of it. Set key to load a
// message; everything else follows.
class MessageView : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loaded)
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(QString subject READ subject NOTIFY loaded)
    Q_PROPERTY(QString fromName READ fromName NOTIFY loaded)
    Q_PROPERTY(QString fromAddress READ fromAddress NOTIFY loaded)
    Q_PROPERTY(QString to READ to NOTIFY loaded)
    Q_PROPERTY(QString cc READ cc NOTIFY loaded)
    Q_PROPERTY(QString date READ date NOTIFY loaded)
    Q_PROPERTY(QString accountName READ accountName NOTIFY loaded)
    Q_PROPERTY(QString html READ html NOTIFY loaded)
    // Mail with its own colours and layout is shown on white paper, as it
    // was designed; plain and simple mail takes the window's colours.
    Q_PROPERTY(bool paper READ paper NOTIFY loaded)
    Q_PROPERTY(int remoteImages READ remoteImages NOTIFY loaded)
    Q_PROPERTY(int trackers READ trackers NOTIFY loaded)
    Q_PROPERTY(bool showPictures READ showPictures WRITE setShowPictures NOTIFY showPicturesChanged)
    Q_PROPERTY(QVariantList attachments READ attachments NOTIFY loaded)
    Q_PROPERTY(QString plainText READ plainText NOTIFY loaded)
    // The event in an invitation, as iCalendar text; empty for other mail.
    Q_PROPERTY(QString calendarData READ calendarData NOTIFY loaded)

public:
    explicit MessageView(QObject *parent = nullptr);

    QString key() const
    {
        return m_key;
    }
    void setKey(const QString &key);
    bool loading() const
    {
        return m_loading;
    }
    QString error() const
    {
        return m_error;
    }
    QString subject() const
    {
        return m_subject;
    }
    QString fromName() const
    {
        return m_fromName;
    }
    QString fromAddress() const
    {
        return m_fromAddress;
    }
    QString to() const
    {
        return m_to;
    }
    QString cc() const
    {
        return m_cc;
    }
    QString date() const;
    QString accountName() const
    {
        return m_accountName;
    }
    QString html() const
    {
        return m_html;
    }
    bool paper() const
    {
        return m_paper;
    }
    int remoteImages() const
    {
        return m_remoteImages;
    }
    int trackers() const
    {
        return m_trackers;
    }
    bool showPictures() const
    {
        return m_showPictures;
    }
    void setShowPictures(bool show);
    QVariantList attachments() const
    {
        return m_attachments;
    }
    QString plainText() const
    {
        return m_plain;
    }
    QString calendarData() const
    {
        return m_calendar;
    }

    // An image the message carries itself (cid: or data:), for HtmlView.
    QImage inlineImage(const QString &src) const;
    // Whether a picture from the web looks like a tracker: so small or
    // hidden it can only be there to report the opening.
    bool isTracker(const QString &src) const
    {
        return m_trackerUrls.contains(src);
    }

    Q_INVOKABLE void openAttachment(int index);
    Q_INVOKABLE void saveAttachment(int index);
    Q_INVOKABLE void saveAllAttachments();
    // To, Cc, subject and the quoted text for a reply ("reply", "replyAll")
    // or a forward ("forward").
    Q_INVOKABLE QVariantMap replyData(const QString &mode) const;
    // The message body can't be selected (litehtml draws it), so its text
    // and links are copied whole.
    Q_INVOKABLE void copyText() const;
    Q_INVOKABLE static void copyToClipboard(const QString &text);

    static QString plainToHtml(const QString &text);

Q_SIGNALS:
    void keyChanged();
    void loaded();
    void showPicturesChanged();
    void attachmentSaved(const QString &path);

private:
    struct Attachment {
        QString name;
        QString mimeType;
        QByteArray data;
    };

    void load();
    QString writeAttachment(int index, const QString &dir) const;

    QString m_key;
    bool m_loading = false;
    QString m_error;
    QString m_subject;
    QString m_fromName;
    QString m_fromAddress;
    QString m_replyTo;
    QString m_to;
    QString m_cc;
    QString m_toAddresses;
    QString m_ccAddresses;
    QString m_messageId;
    QString m_references;
    QDateTime m_date;
    QString m_accountName;
    QString m_html;
    QString m_plain;
    QString m_calendar;
    bool m_paper = false;
    int m_remoteImages = 0;
    int m_trackers = 0;
    bool m_showPictures = false;
    QVariantList m_attachments;
    QList<Attachment> m_attachmentData;
    QHash<QString, QByteArray> m_inline;
    QSet<QString> m_trackerUrls;
    int m_generation = 0;
};
