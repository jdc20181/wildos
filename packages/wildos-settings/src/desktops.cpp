#include "desktops.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QVariantMap>

#include <algorithm>

namespace
{
const QString service = QStringLiteral("org.kde.KWin");
const QString path = QStringLiteral("/VirtualDesktopManager");
const QString interface = QStringLiteral("org.kde.KWin.VirtualDesktopManager");
const QString properties = QStringLiteral("org.freedesktop.DBus.Properties");
}

Desktops::Desktops(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const char *name : {"countChanged", "currentChanged", "desktopsChanged", "desktopCreated", "desktopRemoved", "desktopDataChanged"}) {
        bus.connect(service, path, interface, QString::fromLatin1(name), this, SLOT(reload()));
    }
    reload();
}

void Desktops::reload()
{
    auto message = QDBusMessage::createMethodCall(service, path, properties, QStringLiteral("GetAll"));
    message << interface;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *watcher;
        if (reply.isError()) {
            m_available = false;
            m_desktops.clear();
            m_current.clear();
            Q_EMIT changed();
            return;
        }
        const QVariantMap values = reply.value();

        // desktops is a(uss), KWin's DBusDesktopDataVector: position, id, name.
        QVariantList list;
        const QDBusArgument argument = values.value(QStringLiteral("desktops")).value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            uint position = 0;
            QString id;
            QString name;
            argument.beginStructure();
            argument >> position >> id >> name;
            argument.endStructure();
            list.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), name},
                {QStringLiteral("position"), position},
            });
        }
        argument.endArray();
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("position")).toUInt() < b.toMap().value(QStringLiteral("position")).toUInt();
        });

        m_available = true;
        m_desktops = list;
        m_current = values.value(QStringLiteral("current")).toString();
        Q_EMIT changed();
    });
}

void Desktops::call(const QString &method, const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        reload();
    });
}

// At the end, with no name, so KWin gives it its own "Desktop N" in the
// session's language.
void Desktops::add()
{
    if (m_desktops.size() >= maximum()) {
        return;
    }
    call(QStringLiteral("createDesktop"), {QVariant::fromValue(uint(m_desktops.size())), QString()});
}

void Desktops::rename(const QString &id, const QString &name)
{
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty()) {
        return;
    }
    call(QStringLiteral("setDesktopName"), {id, trimmed});
}

// KWin moves the desktop's windows to the one next to it, as Windows does.
void Desktops::remove(const QString &id)
{
    if (m_desktops.size() <= 1) {
        return;
    }
    call(QStringLiteral("removeDesktop"), {id});
}

void Desktops::switchTo(const QString &id)
{
    auto message = QDBusMessage::createMethodCall(service, path, properties, QStringLiteral("Set"));
    message << interface << QStringLiteral("current") << QVariant::fromValue(QDBusVariant(id));
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        reload();
    });
}
