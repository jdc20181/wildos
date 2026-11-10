#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QQmlEngine>

#include <functional>
#include <optional>

class QLocalServer;
class QLocalSocket;

// How ssh, ssh-keygen and ssh-add ask Remote for a passphrase without one
// ever appearing on a command line (where any process on the PC can read
// it). Remote runs them with SSH_ASKPASS pointing back at itself; that copy
// of Remote (`wildos-remote --askpass PROMPT`) passes the prompt over a
// socket only this user can reach and prints the answer for the tool.
//
// Each run gets a token. An answer comes from the provider registered for
// the token (a passphrase just typed, or one from the keyring), or the
// question goes to the window as passphraseNeeded.
class AskPass : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Returns the answer, or an empty optional to ask the person.
    using Provider = std::function<std::optional<QString>(const QString &prompt, int count)>;

    static AskPass *instance();
    static AskPass *create(QQmlEngine *, QJSEngine *);

    // The environment for a tool run with this token.
    QProcessEnvironment environment(const QString &token, QProcessEnvironment base = QProcessEnvironment::systemEnvironment());
    QString newToken(const Provider &provider = {}, const QString &title = {});
    void release(const QString &token);

    // The helper side, in the copy of Remote ssh starts. Prints the answer
    // and returns 0, or returns 1 when the question was cancelled.
    static int ask(const QString &prompt);

    Q_INVOKABLE void answer(const QString &id, const QString &text);
    Q_INVOKABLE void cancel(const QString &id);

Q_SIGNALS:
    // `kind` is "passphrase", "password", "confirm" (yes or no) or "pin".
    void passphraseNeeded(const QString &id, const QString &title, const QString &prompt, const QString &kind);
    void questionClosed(const QString &id);

private:
    explicit AskPass(QObject *parent = nullptr);
    void listen();
    void incoming();
    void reply(QLocalSocket *socket, bool ok, const QString &text);

    struct Run {
        Provider provider;
        QString title;
        int count = 0;
    };
    QLocalServer *m_server = nullptr;
    QString m_path;
    QHash<QString, Run> m_runs;
    QHash<QString, QPointer<QLocalSocket>> m_waiting;
    int m_next = 1;
};
