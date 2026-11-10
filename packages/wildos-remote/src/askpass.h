#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QVariantMap>

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
// question goes to a window as passphraseNeeded.
class AskPass : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Returns the answer, or an empty optional to ask the person.
    using Provider = std::function<std::optional<QString>(const QString &prompt, int count)>;

    // What a run knows about a question before the person is asked.
    struct Lookup {
        std::optional<QString> answer;
        // Goes with the question to the window that asks.
        QVariantMap details;
    };
    // For a run whose answers come from the keyring, which answers later:
    // done() is called once, perhaps after this returns.
    using AsyncProvider = std::function<void(const QString &prompt, int count, const std::function<void(const Lookup &)> &done)>;
    // The person's answer to one of the run's questions, and whether they
    // ticked "Save in my keyring".
    using Answered = std::function<void(const QString &prompt, const QString &text, bool save)>;

    struct Options {
        AsyncProvider provider;
        Answered answered;
        QString title;
        // The window that asks: empty for the home window, "terminal" for
        // the terminal window.
        QString context;
        // False for a run that must never ask the person: a question the
        // provider can't answer is cancelled.
        bool askPerson = true;
    };

    static AskPass *instance();
    static AskPass *create(QQmlEngine *, QJSEngine *);

    // The environment for a tool run with this token.
    QProcessEnvironment environment(const QString &token, QProcessEnvironment base = QProcessEnvironment::systemEnvironment());
    QString newToken(const Provider &provider = {}, const QString &title = {});
    QString newToken(const Options &options);
    void release(const QString &token);

    // The helper side, in the copy of Remote ssh starts. Prints the answer
    // and returns 0, or returns 1 when the question was cancelled.
    static int ask(const QString &prompt);

    // A password prompt as ssh words it: "alex@web-01's password:", or
    // "Password:" from the server's keyboard-interactive sign-in. A one-time
    // code, a key's passphrase or a new password never is one.
    static bool isPassword(const QString &prompt);

    Q_INVOKABLE void answer(const QString &id, const QString &text, bool save = false);
    Q_INVOKABLE void cancel(const QString &id);

Q_SIGNALS:
    // `kind` is "passphrase", "password", "confirm" (yes or no) or "pin".
    // `context` is the window that asks (see Options), and `details` what
    // the run added to the question.
    void passphraseNeeded(const QString &id, const QString &title, const QString &prompt, const QString &kind, const QString &context, const QVariantMap &details);
    void questionClosed(const QString &id);

private:
    explicit AskPass(QObject *parent = nullptr);
    void listen();
    void incoming();
    void reply(QLocalSocket *socket, bool ok, const QString &text);
    void askPerson(QLocalSocket *socket, const QString &token, const QString &prompt, const QString &kind, const QVariantMap &details);

    struct Run {
        Options options;
        int count = 0;
    };
    struct Waiting {
        QPointer<QLocalSocket> socket;
        QString token;
        QString prompt;
    };
    QLocalServer *m_server = nullptr;
    QString m_path;
    QHash<QString, Run> m_runs;
    QHash<QString, Waiting> m_waiting;
    int m_next = 1;
};
