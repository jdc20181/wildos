#pragma once

#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

namespace Sonnet
{
class Settings;
}

// Spell checking for every app that uses KDE's Sonnet (Kate, KMail, text
// boxes in KDE apps), kept where KDE's own spell checking module keeps it:
// Sonnet::Settings, which writes ~/.config/KDE/Sonnet.conf. Your own words
// are Sonnet's ignore list for the spelling language, plus the words the
// "Add to dictionary" menu item saved in Hunspell's ~/.hunspell_<language>.
class Spelling : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool asYouType READ asYouType NOTIFY changed)
    Q_PROPERTY(bool skipUppercase READ skipUppercase NOTIFY changed)
    Q_PROPERTY(bool autodetect READ autodetect NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QStringList otherLanguages READ otherLanguages NOTIFY changed)
    // [{ value: "en_GB", text: "English (United Kingdom)" }]
    Q_PROPERTY(QVariantList dictionaries READ dictionaries NOTIFY changed)
    Q_PROPERTY(QStringList words READ words NOTIFY changed)

public:
    explicit Spelling(QObject *parent = nullptr);
    ~Spelling() override;

    bool enabled() const;
    bool asYouType() const;
    bool skipUppercase() const;
    bool autodetect() const;
    QString language() const;
    QStringList otherLanguages() const;
    QVariantList dictionaries() const { return m_dictionaries; }
    QStringList words() const { return m_words; }

    Q_INVOKABLE void setEnabled(bool on);
    Q_INVOKABLE void setAsYouType(bool on);
    Q_INVOKABLE void setSkipUppercase(bool on);
    Q_INVOKABLE void setAutodetect(bool on);
    Q_INVOKABLE void setLanguage(const QString &code);
    Q_INVOKABLE void setOtherLanguage(const QString &code, bool on);
    // False when the word is empty, has a space or is already there.
    Q_INVOKABLE bool addWord(const QString &word);
    Q_INVOKABLE void removeWord(const QString &word);

Q_SIGNALS:
    void changed();

private:
    void save();
    void followInBrave();
    void readWords();
    QString personalFile() const;
    QStringList personalWords() const;

    Sonnet::Settings *m_settings;
    QProcess *m_brave = nullptr;
    bool m_braveAgain = false;
    QVariantList m_dictionaries;
    QStringList m_words;
};
