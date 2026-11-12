#include "shellrequests.h"

#include <QDBusConnection>

namespace
{
// Plasma may give applets more than one QML engine, and each engine makes
// its own singleton, but a D-Bus path can be registered only once. One hub
// per process takes the calls and every singleton passes them on.
class Hub : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Shell")

public:
    Hub()
    {
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/wildos/Shell"), this, QDBusConnection::ExportScriptableSlots);
    }

    static Hub *self()
    {
        static Hub *hub = new Hub;
        return hub;
    }

public Q_SLOTS:
    Q_SCRIPTABLE void ToggleAllApps(int x, int y)
    {
        Q_EMIT allApps(x, y);
    }

    Q_SCRIPTABLE void ToggleNotifications(int x, int y)
    {
        Q_EMIT notifications(x, y);
    }

Q_SIGNALS:
    void allApps(int x, int y);
    void notifications(int x, int y);
};
}

ShellRequests::ShellRequests(QObject *parent)
    : QObject(parent)
{
    connect(Hub::self(), &Hub::allApps, this, &ShellRequests::allAppsRequested);
    connect(Hub::self(), &Hub::notifications, this, &ShellRequests::notificationsRequested);
}

#include "shellrequests.moc"
