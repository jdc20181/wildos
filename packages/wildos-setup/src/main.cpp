#include "service.h"

#include <QCoreApplication>
#include <QDBusConnection>

#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-setup-helper"));

    if (::geteuid() != 0) {
        qCritical("wildos-setup-helper must run as root, started by the system bus");
        return 1;
    }
    // useradd and friends write files that must not be group or world writable.
    ::umask(022);

    QDBusConnection bus = QDBusConnection::systemBus();
    SetupService service;
    if (!bus.registerObject(QStringLiteral("/org/wildos/Setup1"), &service, QDBusConnection::ExportAllSlots)
        || !bus.registerService(QStringLiteral("org.wildos.Setup1"))) {
        qCritical("could not take org.wildos.Setup1 on the system bus: %s", qPrintable(bus.lastError().message()));
        return 1;
    }
    return app.exec();
}
