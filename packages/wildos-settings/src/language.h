#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// Display language and regional format, kept where KDE's region module
// keeps them: plasma-localerc, which startplasma turns into LANGUAGE, LANG
// and the LC_ variables at the next sign-in. A locale the system has not
// generated yet is generated first through
// /usr/libexec/wildos/settings-locale under pkexec, as glibc cannot use it
// otherwise.
class Language : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QString format READ format NOTIFY changed)
    Q_PROPERTY(bool needsSignOut READ needsSignOut NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool detailed READ detailed NOTIFY changed)

public:
    explicit Language(QObject *parent = nullptr);

    QString language() const { return m_language; }
    QString format() const { return m_format; }
    bool needsSignOut() const;
    bool busy() const { return m_busy; }
    bool detailed() const { return m_detailed; }

    // [{ value: "de", text: "Deutsch (Deutschland)", english: "German" }],
    // the languages the desktop has translations for, UK English first.
    Q_INVOKABLE QVariantList languages() const;
    // [{ value: "en_GB.UTF-8", text: "English (United Kingdom)" }], every
    // UTF-8 locale glibc supports, UK English first.
    Q_INVOKABLE QVariantList formats() const;
    // How dates, times, numbers and money look in a format.
    Q_INVOKABLE QVariantMap examples(const QString &format) const;
    Q_INVOKABLE void setLanguage(const QString &code);
    Q_INVOKABLE void setFormat(const QString &locale);
    Q_INVOKABLE void signOut();

Q_SIGNALS:
    void changed();
    void busyChanged();
    void failed(const QString &reason);

private:
    void read();
    void ensure(const QStringList &locales, const std::function<void()> &then);

    QString m_language;
    QString m_format;
    QString m_sessionLanguage;
    QString m_sessionFormat;
    bool m_detailed = false;
    bool m_busy = false;
};
