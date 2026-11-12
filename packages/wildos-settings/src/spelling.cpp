#include "spelling.h"

#include <Sonnet/Settings>
#include <Sonnet/Speller>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QTextStream>

namespace
{
// Hunspell's "Add to dictionary" file (sonnet hunspelldict.cpp): a word on
// each line, "word/flags" with affix flags, "*word" for one taken out.
QString baseWord(const QString &line)
{
    if (line.isEmpty() || line.startsWith(QLatin1Char('*'))) {
        return {};
    }
    return line.section(QLatin1Char('/'), 0, 0).trimmed();
}
}

Spelling::Spelling(QObject *parent)
    : QObject(parent)
    , m_settings(new Sonnet::Settings(this))
{
    const QMap<QString, QString> available = Sonnet::Speller().availableDictionaries();
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    for (auto it = available.cbegin(); it != available.cend(); ++it) {
        m_dictionaries << QVariantMap{{QStringLiteral("value"), it.value()}, {QStringLiteral("text"), it.key()}};
    }
    std::sort(m_dictionaries.begin(), m_dictionaries.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("text")).toString(), b.toMap().value(QStringLiteral("text")).toString()) < 0;
    });
    readWords();
}

Spelling::~Spelling()
{
    // Closing Settings straight after a change would otherwise kill the
    // hand-over to Brave half way.
    if (m_brave && m_brave->state() != QProcess::NotRunning) {
        m_brave->waitForFinished(5000);
    }
}

bool Spelling::enabled() const
{
    return m_settings->checkerEnabledByDefault();
}

bool Spelling::asYouType() const
{
    return m_settings->backgroundCheckerEnabled();
}

bool Spelling::skipUppercase() const
{
    return m_settings->skipUppercase();
}

bool Spelling::autodetect() const
{
    return m_settings->autodetectLanguage();
}

QString Spelling::language() const
{
    return m_settings->defaultLanguage();
}

QStringList Spelling::otherLanguages() const
{
    return m_settings->preferredLanguages();
}

void Spelling::save()
{
    // Sonnet has no change signal for other apps; each reads Sonnet.conf
    // when it opens a text box, as with KDE's own module.
    m_settings->save();
    Q_EMIT changed();
}

// Brave takes its spelling languages from a policy only root can write;
// wildos-brave-spelling asks for it. One at a time, so quick changes in a
// row cannot land out of order.
void Spelling::followInBrave()
{
    if (!m_brave) {
        m_brave = new QProcess(this);
        m_brave->setProgram(QStringLiteral("/usr/libexec/wildos/wildos-brave-spelling"));
        connect(m_brave, &QProcess::finished, this, [this] {
            if (m_braveAgain) {
                m_braveAgain = false;
                m_brave->start();
            }
        });
    }
    if (m_brave->state() != QProcess::NotRunning) {
        m_braveAgain = true;
        return;
    }
    m_brave->start();
}

void Spelling::setEnabled(bool on)
{
    m_settings->setCheckerEnabledByDefault(on);
    save();
}

void Spelling::setAsYouType(bool on)
{
    m_settings->setBackgroundCheckerEnabled(on);
    save();
}

void Spelling::setSkipUppercase(bool on)
{
    m_settings->setSkipUppercase(on);
    save();
}

void Spelling::setAutodetect(bool on)
{
    m_settings->setAutodetectLanguage(on);
    save();
    followInBrave();
}

void Spelling::setLanguage(const QString &code)
{
    if (code.isEmpty() || code == m_settings->defaultLanguage()) {
        return;
    }
    m_settings->setDefaultLanguage(code);
    if (m_settings->defaultLanguage() != code) {
        return;
    }
    // Sonnet keeps one ignore list a language but loads only the first
    // one; without this it would save the old language's words under the
    // new one.
    const QSettings stored(QStringLiteral("KDE"), QStringLiteral("Sonnet"));
    m_settings->setCurrentIgnoreList(stored.value(QStringLiteral("ignore_%1").arg(code), Sonnet::Settings::defaultIgnoreList()).toStringList());
    save();
    readWords();
    followInBrave();
}

void Spelling::setOtherLanguage(const QString &code, bool on)
{
    QStringList list = m_settings->preferredLanguages();
    if (on == list.contains(code)) {
        return;
    }
    if (on) {
        list << code;
    } else {
        list.removeAll(code);
    }
    m_settings->setPreferredLanguages(list);
    save();
    followInBrave();
}

QString Spelling::personalFile() const
{
    return QDir::home().filePath(QStringLiteral(".hunspell_") + m_settings->defaultLanguage());
}

QStringList Spelling::personalWords() const
{
    QStringList list;
    QFile file(personalFile());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString word = baseWord(in.readLine());
            if (!word.isEmpty()) {
                list << word;
            }
        }
    }
    return list;
}

void Spelling::readWords()
{
    // Sonnet's built-in list of KDE product names is not something you
    // added, so it stays in force without being listed.
    const QStringList builtIn = Sonnet::Settings::defaultIgnoreList();
    QSet<QString> seen;
    QStringList list;
    for (const QString &word : m_settings->currentIgnoreList() + personalWords()) {
        if (!builtIn.contains(word) && !seen.contains(word)) {
            seen.insert(word);
            list << word;
        }
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    m_words = list;
    Q_EMIT changed();
}

bool Spelling::addWord(const QString &text)
{
    const QString word = text.trimmed();
    if (word.isEmpty() || word.contains(QLatin1Char(' ')) || m_words.contains(word)) {
        return false;
    }
    QStringList ignored = m_settings->currentIgnoreList();
    if (!ignored.contains(word)) {
        ignored << word;
        m_settings->setCurrentIgnoreList(ignored);
        m_settings->save();
    }
    readWords();
    return true;
}

void Spelling::removeWord(const QString &word)
{
    QStringList ignored = m_settings->currentIgnoreList();
    if (ignored.removeAll(word) > 0) {
        m_settings->setCurrentIgnoreList(ignored);
        m_settings->save();
    }

    QFile file(personalFile());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList kept;
        bool found = false;
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (baseWord(line) == word) {
                found = true;
            } else {
                kept << line;
            }
        }
        file.close();
        if (found) {
            QSaveFile out(personalFile());
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&out);
                for (const QString &line : std::as_const(kept)) {
                    stream << line << '\n';
                }
                stream.flush();
                out.commit();
            }
        }
    }
    readWords();
}
