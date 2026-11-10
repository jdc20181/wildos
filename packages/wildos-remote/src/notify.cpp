#include "notify.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace
{
const QString service = QStringLiteral("org.freedesktop.Notifications");
const QString path = QStringLiteral("/org/freedesktop/Notifications");
}

Notify *Notify::instance()
{
    static Notify *n = new Notify;
    return n;
}

Notify *Notify::create(QQmlEngine *, QJSEngine *)
{
    Notify *n = instance();
    QJSEngine::setObjectOwnership(n, QJSEngine::CppOwnership);
    return n;
}

Notify::Notify(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(service, path, service, QStringLiteral("ActionInvoked"), this, SLOT(invoked(uint, QString)));
}

void Notify::send(const QString &title, const QString &body, const QString &action, const QJSValue &onAction)
{
    QDBusMessage m = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("Notify"));
    const QStringList actions = action.isEmpty() ? QStringList() : QStringList{QStringLiteral("default"), action, QStringLiteral("act"), action};
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("org.wildos.remote"));
    m << QStringLiteral("Remote") << uint(0) << QStringLiteral("org.wildos.remote") << title << body << actions << hints << int(-1);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, onAction] {
        QDBusPendingReply<uint> reply = *watcher;
        if (!reply.isError() && onAction.isCallable()) {
            m_actions.insert(reply.value(), onAction);
        }
        watcher->deleteLater();
    });
}

void Notify::invoked(uint id, const QString &)
{
    QJSValue callback = m_actions.take(id);
    if (callback.isCallable()) {
        callback.call();
    }
}
