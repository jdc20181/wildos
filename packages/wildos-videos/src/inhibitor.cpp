#include "inhibitor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

Inhibitor::Inhibitor(QObject *parent)
    : QObject(parent)
{
    m_screen = {QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/org/freedesktop/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver")};
    m_sleep = {QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
               QStringLiteral("org.freedesktop.PowerManagement.Inhibit")};
}

Inhibitor::~Inhibitor()
{
    m_active = false;
    release(m_screen);
    release(m_sleep);
}

void Inhibitor::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (active) {
        take(m_screen);
        take(m_sleep);
    } else {
        release(m_screen);
        release(m_sleep);
    }
}

void Inhibitor::take(Hold &hold)
{
    if (hold.cookie != 0 || hold.waiting) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(hold.service, hold.path, hold.interface, QStringLiteral("Inhibit"));
    message << QStringLiteral("Videos") << tr("Playing a video");
    hold.waiting = true;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, &hold](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        hold.waiting = false;
        const QDBusPendingReply<uint> reply = *call;
        if (reply.isError()) {
            return;
        }
        hold.cookie = reply.value();
        // Paused again before the answer came.
        if (!m_active) {
            release(hold);
        }
    });
}

void Inhibitor::release(Hold &hold)
{
    if (hold.cookie == 0) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(hold.service, hold.path, hold.interface, QStringLiteral("UnInhibit"));
    message << hold.cookie;
    hold.cookie = 0;
    QDBusConnection::sessionBus().send(message);
}
