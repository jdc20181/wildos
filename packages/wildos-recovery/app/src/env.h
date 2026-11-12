#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// The recovery system's root side (/usr/libexec/wildos/recovery-env),
// through sudo: the live user may run it without a password, and anything
// that changes the disk asks for the disk password in the app first.
class Env : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY infoChanged)

public:
    explicit Env(QObject *parent = nullptr);

    QVariantMap info() const { return m_info; }
    bool loaded() const { return m_loaded; }

    Q_INVOKABLE void refresh();
    // Runs `recovery-env <args>`, with `input` on its stdin; the answer
    // arrives as replied(tag, answer).
    Q_INVOKABLE void call(const QString &tag, const QStringList &args, const QString &input = QString());
    Q_INVOKABLE void openTerminal();
    // Remove everything: the installer, once the disk is closed.
    Q_INVOKABLE void openInstaller();
    Q_INVOKABLE void nextLayout();
    Q_INVOKABLE QString layoutName() const;

Q_SIGNALS:
    void infoChanged();
    void replied(const QString &tag, const QVariantMap &answer);
    void layoutChanged();

private:
    QVariantMap m_info;
    bool m_loaded = false;
};
