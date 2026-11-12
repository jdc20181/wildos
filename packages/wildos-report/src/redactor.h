#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// Takes the details that could identify someone out of a report before
// anyone sees it, and puts a token in their place: [user], [name], [pc],
// [wifi-1], [ip-1], [mac-1], [serial-1], [id-1], [email-1], [file-1],
// [folder-1], [usb-1] and [word-1] for words the person adds. The same value
// gets the same token everywhere in the report, so two Wi-Fi networks stay
// [wifi-1] and [wifi-2] and a log still reads.
//
// Plain text in, plain text out: no Qt GUI, so the tests and the start-up
// recovery tool use it as it is.
class Redactor
{
public:
    // The groups the app shows as chips under "personal details removed".
    enum Category {
        Names,
        PcName,
        Wifi,
        Addresses,
        Serials,
        Emails,
        Files,
        Custom,
        CategoryCount,
    };

    // What this PC knows about the people on it: everything here is removed
    // wherever it appears, on top of what the patterns find.
    struct Known {
        QString userName;
        QString realName;
        // Other accounts on this PC, by user name and real name.
        QStringList otherUsers;
        QStringList otherNames;
        QString hostName;
        QStringList wifi;
        QStringList serials;
        QStringList machineIds;
    };

    void setKnown(const Known &known);
    void setCustomWords(const QStringList &words);

    // Starts a new report: tokens are numbered from 1 again and the counts
    // go back to nothing. Redact every part of one report between resets so
    // their tokens agree.
    void reset();

    QString redact(const QString &text);

    // How many details were replaced since the last reset, in all and by
    // category.
    int total() const;
    int count(Category category) const;

    static QString categoryLabel(Category category);

private:
    struct Span {
        qsizetype start;
        qsizetype length;
        QString kind;
        QString value;
        Category category;
        bool numbered;
    };

    QString token(const QString &kind, const QString &value, Category category, bool numbered = true);
    void claim(QList<Span> &spans, QList<bool> &taken, qsizetype start, qsizetype length, const QString &kind, const QString &value, Category category,
               bool numbered = true);
    void literal(const QString &text, QList<Span> &spans, QList<bool> &taken, const QString &value, const QString &kind, Category category,
                 Qt::CaseSensitivity cs, bool numbered);
    void paths(const QString &text, QList<Span> &spans, QList<bool> &taken);
    void pathSpans(const QString &text, qsizetype start, qsizetype length, QList<Span> &spans, QList<bool> &taken);

    Known m_known;
    QStringList m_custom;
    // kind -> value (folded) -> token
    QHash<QString, QHash<QString, QString>> m_tokens;
    int m_counts[CategoryCount] = {};
};
