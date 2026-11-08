#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// The few things a control needs from the system beyond drawing: whether a
// Wild OS tool is installed and starting it (the text menu's Emoji row
// opens the emoji panel), and the window menu a right click on a title bar
// opens, which Wild OS's KWin plugin draws and serves on the session bus,
// and a person's password hint.
class System : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit System(QObject *parent = nullptr);

    Q_INVOKABLE bool canRun(const QString &program) const;
    Q_INVOKABLE bool run(const QString &program, const QStringList &arguments = {}) const;
    Q_INVOKABLE bool hasWindowMenu() const;
    Q_INVOKABLE void showWindowMenu() const;
    // The hint a person set with their password (Welcome, Settings), shown
    // by the sign-in and lock screens after a wrong attempt.
    Q_INVOKABLE QString passwordHint(const QString &userName = {}) const;
};
