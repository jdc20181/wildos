#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>

// Advice on a password someone is choosing, from libpwquality's checks and
// score. It never decides whether a password can be used: a password is a
// choice, and the dialogs keep only their own rules. The password goes to
// libpwquality inside this process and nowhere else; it is not kept, logged
// or written down.
class PasswordCheck : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // { level: "weak", "fair" or "strong", reason: why it is weak, or "" }.
    // `names` are the account's user name and full name; the first is
    // passed to libpwquality as the user name when it is one word.
    Q_INVOKABLE QVariantMap check(const QString &password, const QString &oldPassword, const QStringList &names) const;
};
