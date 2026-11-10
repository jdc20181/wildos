#include "secrets.h"

#include <qt6keychain/keychain.h>

namespace
{
const QString service = QStringLiteral("Wild OS Remote");
}

Secrets *Secrets::instance()
{
    static Secrets *secrets = new Secrets;
    return secrets;
}

Secrets *Secrets::create(QQmlEngine *, QJSEngine *)
{
    Secrets *secrets = instance();
    QJSEngine::setObjectOwnership(secrets, QJSEngine::CppOwnership);
    return secrets;
}

Secrets::Secrets(QObject *parent)
    : QObject(parent)
{
}

void Secrets::read(const QString &key, const std::function<void(const QString &, bool)> &done)
{
    auto *job = new QKeychain::ReadPasswordJob(service, this);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [job, done]() {
        const bool found = job->error() == QKeychain::NoError;
        done(found ? job->textData() : QString(), found);
    });
    job->start();
}

void Secrets::write(const QString &key, const QString &secret, const std::function<void(bool)> &done)
{
    auto *job = new QKeychain::WritePasswordJob(service, this);
    job->setKey(key);
    job->setTextData(secret);
    connect(job, &QKeychain::Job::finished, this, [job, done]() {
        if (job->error() != QKeychain::NoError) {
            qWarning("wildos-remote: the keyring did not take a secret: %s", qPrintable(job->errorString()));
        }
        if (done) {
            done(job->error() == QKeychain::NoError);
        }
    });
    job->start();
}

void Secrets::remove(const QString &key)
{
    auto *job = new QKeychain::DeletePasswordJob(service, this);
    job->setKey(key);
    job->start();
}

void Secrets::read(const QString &key, const QJSValue &callback)
{
    read(key, [callback](const QString &secret, bool found) mutable {
        if (callback.isCallable()) {
            callback.call({QJSValue(secret), QJSValue(found)});
        }
    });
}

void Secrets::write(const QString &key, const QString &secret, const QJSValue &callback)
{
    write(key, secret, [callback](bool ok) mutable {
        if (callback.isCallable()) {
            callback.call({QJSValue(ok)});
        }
    });
}

void Secrets::exists(const QString &key, const QJSValue &callback)
{
    read(key, [callback](const QString &, bool found) mutable {
        if (callback.isCallable()) {
            callback.call({QJSValue(found)});
        }
    });
}
