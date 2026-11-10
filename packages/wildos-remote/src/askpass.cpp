#include "askpass.h"
#include "paths.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTextStream>
#include <QUuid>

#include <cstdio>

namespace
{
const char *tokenVariable = "WILDOS_REMOTE_ASKPASS_TOKEN";
const char *socketVariable = "WILDOS_REMOTE_ASKPASS_SOCKET";
}

AskPass *AskPass::instance()
{
    static AskPass *askPass = new AskPass;
    return askPass;
}

AskPass *AskPass::create(QQmlEngine *, QJSEngine *)
{
    AskPass *a = instance();
    QJSEngine::setObjectOwnership(a, QJSEngine::CppOwnership);
    return a;
}

AskPass::AskPass(QObject *parent)
    : QObject(parent)
{
}

void AskPass::listen()
{
    if (m_server) {
        return;
    }
    m_path = Paths::runtime() + QStringLiteral("/askpass-%1").arg(QCoreApplication::applicationPid());
    QLocalServer::removeServer(m_path);
    m_server = new QLocalServer(this);
    // Only this user: the folder already says so, and the socket agrees.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_path)) {
        qWarning("wildos-remote: could not listen for passphrase questions: %s", qPrintable(m_server->errorString()));
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, &AskPass::incoming);
}

QString AskPass::newToken(const Provider &provider, const QString &title)
{
    listen();
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runs.insert(token, {provider, title, 0});
    return token;
}

void AskPass::release(const QString &token)
{
    m_runs.remove(token);
}

QProcessEnvironment AskPass::environment(const QString &token, QProcessEnvironment env)
{
    listen();
    env.insert(QStringLiteral("SSH_ASKPASS"), QCoreApplication::applicationFilePath());
    env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    // OpenSSH only uses askpass with a display to show it on; the helper
    // needs none, but a session without either would never get asked.
    if (!env.contains(QStringLiteral("DISPLAY")) && !env.contains(QStringLiteral("WAYLAND_DISPLAY"))) {
        env.insert(QStringLiteral("DISPLAY"), QStringLiteral(":0"));
    }
    env.insert(QString::fromLatin1(tokenVariable), token);
    env.insert(QString::fromLatin1(socketVariable), m_path);
    return env;
}

void AskPass::incoming()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (!socket->canReadLine()) {
                return;
            }
            const QJsonObject q = QJsonDocument::fromJson(socket->readLine()).object();
            const QString token = q.value(QStringLiteral("token")).toString();
            const QString prompt = q.value(QStringLiteral("prompt")).toString().trimmed();
            const QString kind = q.value(QStringLiteral("kind")).toString();
            auto run = m_runs.find(token);
            if (run == m_runs.end()) {
                // Not a run Remote started: nothing to answer with.
                reply(socket, false, {});
                return;
            }
            ++run->count;
            if (run->provider && kind != QLatin1String("touch")) {
                if (const auto answer = run->provider(prompt, run->count)) {
                    reply(socket, true, *answer);
                    return;
                }
            }
            const QString id = QString::number(m_next++);
            m_waiting.insert(id, socket);
            // The tool gave up waiting, or (for a touch notice) is done.
            connect(socket, &QLocalSocket::disconnected, this, [this, id] {
                if (m_waiting.remove(id)) {
                    Q_EMIT questionClosed(id);
                }
            });
            QString shown = kind;
            if (shown.isEmpty()) {
                shown = prompt.contains(QLatin1String("passphrase"), Qt::CaseInsensitive) ? QStringLiteral("passphrase")
                        : prompt.contains(QLatin1String("PIN"))                         ? QStringLiteral("pin")
                                                                                         : QStringLiteral("password");
            }
            Q_EMIT passphraseNeeded(id, run->title, prompt, shown);
        });
    }
}

void AskPass::reply(QLocalSocket *socket, bool ok, const QString &text)
{
    QJsonObject a;
    a.insert(QStringLiteral("ok"), ok);
    a.insert(QStringLiteral("text"), text);
    socket->write(QJsonDocument(a).toJson(QJsonDocument::Compact) + '\n');
    socket->flush();
}

void AskPass::answer(const QString &id, const QString &text)
{
    if (QLocalSocket *socket = m_waiting.take(id)) {
        reply(socket, true, text);
    }
}

void AskPass::cancel(const QString &id)
{
    if (QLocalSocket *socket = m_waiting.take(id)) {
        reply(socket, false, {});
    }
}

int AskPass::ask(const QString &prompt)
{
    const QString path = qEnvironmentVariable(socketVariable);
    const QString token = qEnvironmentVariable(tokenVariable);
    if (path.isEmpty() || token.isEmpty()) {
        QTextStream(stderr) << "wildos-remote --askpass only answers for Remote\n";
        return 1;
    }
    // SSH_ASKPASS_PROMPT is "confirm" for a yes or no question and "none"
    // for a notice that stays up until ssh is done (touch your security
    // key).
    const QString mode = qEnvironmentVariable("SSH_ASKPASS_PROMPT");
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(3000)) {
        return 1;
    }
    QJsonObject q;
    q.insert(QStringLiteral("token"), token);
    q.insert(QStringLiteral("prompt"), prompt);
    q.insert(QStringLiteral("kind"), mode == QLatin1String("confirm") ? QStringLiteral("confirm") : mode == QLatin1String("none") ? QStringLiteral("touch") : QString());
    socket.write(QJsonDocument(q).toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(-1)) {
            return 1;
        }
    }
    const QJsonObject a = QJsonDocument::fromJson(socket.readLine()).object();
    if (!a.value(QStringLiteral("ok")).toBool()) {
        return 1;
    }
    if (mode != QLatin1String("confirm") && mode != QLatin1String("none")) {
        const QByteArray text = a.value(QStringLiteral("text")).toString().toUtf8();
        std::fwrite(text.constData(), 1, size_t(text.size()), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    return 0;
}
