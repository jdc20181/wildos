#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

// Finds the server settings for an email address, so people type their
// address and password and nothing else. Known providers are built in;
// for the rest it asks the domain's own autoconfig file, then Thunderbird's
// public database (which learns only the domain, never the address), then
// guesses the usual names.
class AutoConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit AutoConfig(QObject *parent = nullptr);

    bool busy() const
    {
        return m_busy;
    }

    // Answers through found(), always: with a guess when nothing better
    // turned up.
    Q_INVOKABLE void lookup(const QString &email);
    Q_INVOKABLE static QVariantMap preset(const QString &provider, const QString &email);
    Q_INVOKABLE static QString providerOf(const QString &email);

Q_SIGNALS:
    void busyChanged();
    void found(const QVariantMap &settings, bool guessed);

private:
    void tryUrl(const QStringList &urls, const QString &email);
    void tryMx(const QString &email);
    void finish(const QVariantMap &settings, bool guessed);

    QNetworkAccessManager *m_network;
    bool m_busy = false;
    int m_generation = 0;
};
