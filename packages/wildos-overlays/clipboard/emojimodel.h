#pragma once

#include <QJsonArray>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Every emoji, from Unicode's emoji list and CLDR's English names and
// search words (built into the program by tools/make-emoji.py), in the
// panel's groups. Each emoji is [text, name, search words, five skin tone
// variants or nothing].
class EmojiData : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList groups READ groups CONSTANT)

public:
    explicit EmojiData(QObject *parent = nullptr);

    QVariantList groups() const { return m_groups; }

    // Emoji whose name or search words start with every word typed.
    Q_INVOKABLE QVariantList search(const QString &query, int limit = 160) const;
    Q_INVOKABLE QVariantList find(const QStringList &texts) const;

private:
    QVariantList m_groups;
    QJsonArray m_all;
};
