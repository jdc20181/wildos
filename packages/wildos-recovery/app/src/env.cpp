#include "env.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace
{
const QString helper = QStringLiteral("/usr/libexec/wildos/recovery-env");

QVariantMap parse(const QByteArray &output)
{
    const QList<QByteArray> lines = output.trimmed().split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QJsonDocument doc = QJsonDocument::fromJson(*it);
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return {{QStringLiteral("ok"), false}, {QStringLiteral("why"), QString::fromUtf8(output.trimmed().right(200))}};
}
}

Env::Env(QObject *parent)
    : QObject(parent)
{
    connect(this, &Env::replied, this, [this](const QString &tag, const QVariantMap &answer) {
        if (tag == QLatin1String("info")) {
            m_info = answer;
            m_loaded = true;
            Q_EMIT infoChanged();
        }
    });
    refresh();
}

void Env::refresh()
{
    call(QStringLiteral("info"), {QStringLiteral("info")});
}

void Env::call(const QString &tag, const QStringList &args, const QString &input)
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, tag](int, QProcess::ExitStatus) {
        const QVariantMap answer = parse(process->readAllStandardOutput());
        process->deleteLater();
        Q_EMIT replied(tag, answer);
    });
    process->start(QStringLiteral("sudo"), QStringList{QStringLiteral("-n"), helper} + args);
    if (!input.isNull()) {
        process->write(input.toUtf8());
    }
    process->closeWriteChannel();
}

void Env::openTerminal()
{
    for (const QString &terminal : {QStringLiteral("wildos-terminal"), QStringLiteral("konsole")}) {
        const QString path = QStandardPaths::findExecutable(terminal);
        if (!path.isEmpty()) {
            QProcess::startDetached(path, {});
            return;
        }
    }
}

void Env::openInstaller()
{
    QProcess::startDetached(QStringLiteral("/usr/bin/install-wildos"), {});
}

void Env::nextLayout()
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"),
                                                       QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("switchToNextLayout"));
    QDBusConnection::sessionBus().call(call);
    Q_EMIT layoutChanged();
}

QString Env::layoutName() const
{
    QDBusMessage index = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"),
                                                        QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("getLayout"));
    QDBusReply<uint> current = QDBusConnection::sessionBus().call(index);
    QDBusMessage list = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"),
                                                       QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("getLayoutsList"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(list);
    if (current.isValid() && reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        // a(sss): short name, variant, long name.
        const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
        QStringList names;
        arg.beginArray();
        while (!arg.atEnd()) {
            QString shortName, variant, longName;
            arg.beginStructure();
            arg >> shortName >> variant >> longName;
            arg.endStructure();
            names << longName;
        }
        arg.endArray();
        if (current.value() < uint(names.size())) {
            return names.at(int(current.value()));
        }
    }
    return {};
}
