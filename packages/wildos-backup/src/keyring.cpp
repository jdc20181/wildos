#include "keyring.h"
#include "paths.h"

#include <QEventLoop>
#include <QRandomGenerator>
#include <QTimer>

#include <qt6keychain/keychain.h>

namespace Keyring
{
void read(QObject *context, ReadDone done)
{
    auto *job = new QKeychain::ReadPasswordJob(QString::fromLatin1(Paths::keyringService()), context);
    job->setKey(QString::fromLatin1(Paths::keyringKey()));
    job->setAutoDelete(true);
    QObject::connect(job, &QKeychain::Job::finished, context, [job, done]() {
        if (job->error() == QKeychain::NoError) {
            done(true, job->binaryData(), false, QString());
        } else {
            done(false, QByteArray(), job->error() == QKeychain::EntryNotFound, job->errorString());
        }
    });
    job->start();
}

void write(QObject *context, const QByteArray &password, WriteDone done)
{
    auto *job = new QKeychain::WritePasswordJob(QString::fromLatin1(Paths::keyringService()), context);
    job->setKey(QString::fromLatin1(Paths::keyringKey()));
    job->setBinaryData(password);
    job->setAutoDelete(true);
    QObject::connect(job, &QKeychain::Job::finished, context, [job, done]() {
        done(job->error() == QKeychain::NoError, job->errorString());
    });
    job->start();
}

bool readBlocking(QByteArray *password, QString *why)
{
    QObject context;
    QEventLoop loop;
    bool ok = false;
    read(&context, [&](bool good, const QByteArray &value, bool missing, const QString &reason) {
        ok = good;
        if (good) {
            *password = value;
        } else if (why) {
            *why = missing ? QStringLiteral("missing") : reason;
        }
        loop.quit();
    });
    // A keyring that asks to be unlocked can take a while to answer.
    QTimer::singleShot(120000, &loop, &QEventLoop::quit);
    loop.exec();
    return ok && !password->isEmpty();
}

QByteArray generate()
{
    static const char alphabet[] = "abcdefghijkmnpqrstuvwxyz23456789";
    constexpr int letters = sizeof(alphabet) - 1;
    QByteArray out;
    auto *random = QRandomGenerator::system();
    for (int i = 0; i < 24; ++i) {
        if (i > 0 && i % 4 == 0) {
            out.append('-');
        }
        out.append(alphabet[random->bounded(letters)]);
    }
    return out;
}
}
