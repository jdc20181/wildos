#include "poweroptions.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

PowerOptions::PowerOptions(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void PowerOptions::refresh()
{
    auto lid = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
                                              QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    lid << QStringLiteral("org.freedesktop.UPower") << QStringLiteral("LidIsPresent");
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(lid), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<QDBusVariant> reply = *watcher;
        m_lidPresent = !reply.isError() && reply.value().variant().toBool();
        Q_EMIT changed();
    });

    ask(QStringLiteral("CanSuspend"), &m_canSuspend);
    ask(QStringLiteral("CanHibernate"), &m_canHibernate);
}

// Asked of powerdevil rather than logind directly: its settings module
// offers sleep and hibernate on the same answers, and they are the ones
// its actions obey.
void PowerOptions::ask(const QString &method, bool *target)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PowerManagement"), QStringLiteral("/org/freedesktop/PowerManagement"),
                                                  QStringLiteral("org.freedesktop.PowerManagement"), method);
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, target] {
        watcher->deleteLater();
        QDBusPendingReply<bool> reply = *watcher;
        *target = !reply.isError() && reply.value();
        Q_EMIT changed();
    });
}
