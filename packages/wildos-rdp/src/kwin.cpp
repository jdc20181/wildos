#include "kwin.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

namespace
{
// The window is found by this process, its app id and its size now; the
// script runs once, inside KWin, and is unloaded again.
const char *script = R"(import QtQuick
import org.kde.kwin as KWin

Item {
    Component.onCompleted: {
        const want = %1;
        for (const w of KWin.Workspace.windows) {
            const g = w.frameGeometry;
            if (w.deleted || w.pid !== want.pid || String(w.desktopFileName) !== want.app
                || Math.round(g.width) !== want.from[0] || Math.round(g.height) !== want.from[1]) {
                continue;
            }
            let x = g.x + (g.width - want.to[0]) / 2;
            let y = g.y + (g.height - want.to[1]) / 2;
            const area = KWin.Workspace.clientArea(KWin.Workspace.PlacementArea, w);
            if (area && area.width > 0) {
                x = Math.max(area.x, Math.min(x, area.x + area.width - Math.min(want.to[0], area.width)));
                y = Math.max(area.y, Math.min(y, area.y + area.height - Math.min(want.to[1], area.height)));
            }
            w.frameGeometry = Qt.rect(Math.round(x), Math.round(y), want.to[0], want.to[1]);
            break;
        }
    }
}
)";

QDBusMessage kwinCall(const QString &path, const QString &interface, const QString &method, const QVariantList &args)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, interface, method);
    m.setArguments(args);
    return m;
}
}

bool KWinPlace::resizeAroundCentre(QWindow *window, const QString &appId, const QSize &to)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty() || !window->isVisible()) {
        return false;
    }
    static int serial = 0;
    const QString name = QStringLiteral("wildos-rdp-centre-%1-%2").arg(QCoreApplication::applicationPid()).arg(++serial);
    const QString path = dir + u'/' + name + QStringLiteral(".qml");
    QJsonObject want;
    want.insert(QStringLiteral("pid"), qint64(QCoreApplication::applicationPid()));
    want.insert(QStringLiteral("app"), appId);
    want.insert(QStringLiteral("from"), QJsonArray{window->width(), window->height()});
    want.insert(QStringLiteral("to"), QJsonArray{to.width(), to.height()});
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QString::fromLatin1(script).arg(QString::fromUtf8(QJsonDocument(want).toJson(QJsonDocument::Compact))).toUtf8());
    if (!file.commit()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QDBusMessage loaded = bus.call(kwinCall(QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"),
                                                  QStringLiteral("loadDeclarativeScript"), {path, name}),
                                         QDBus::Block, 500);
    const int id = loaded.arguments().value(0).toInt();
    bool ok = loaded.type() == QDBusMessage::ReplyMessage && id >= 0;
    if (ok) {
        const QDBusMessage ran = bus.call(kwinCall(QStringLiteral("/Scripting/Script%1").arg(id), QStringLiteral("org.kde.kwin.Script"),
                                                   QStringLiteral("run"), {}),
                                          QDBus::Block, 500);
        ok = ran.type() == QDBusMessage::ReplyMessage;
    }
    // KWin has read the file once the script is running; the script is
    // unloaded when it has had time to run.
    QTimer::singleShot(2000, [name, path] {
        QDBusConnection::sessionBus().asyncCall(
            kwinCall(QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("unloadScript"), {name}));
        QFile::remove(path);
    });
    return ok;
}
