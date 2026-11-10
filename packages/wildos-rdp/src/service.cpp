#include "service.h"
#include "client.h"
#include "rail.h"
#include "railwindow.h"

#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include <freerdp/rail.h>

Service::Service(Rail *rail, Client *client, QObject *parent)
    : QObject(parent)
    , m_rail(rail)
    , m_client(client)
{
    connect(rail, &Rail::windowShown, this, [this](quint32 id, const QString &title) { Q_EMIT WindowAdded(id, title); });
    connect(rail, &Rail::windowGone, this, [this](quint32 id) { Q_EMIT WindowRemoved(id); });
    connect(rail, &Rail::loadingVisible, this, [this](quint32 token, qint64 ms) { Q_EMIT LoadingShown(token, uint(ms)); });
    connect(rail, &Rail::closeStalled, this, [this](quint32 id, quint32 pid, const QString &program) { Q_EMIT CloseStalled(id, pid, program); });
    connect(rail, &Rail::loadingDetails, this, [this](quint32 token) { Q_EMIT DetailsRequested(token); });
    connect(rail, &Rail::loadingRetry, this, [this](quint32 token) { Q_EMIT RetryRequested(token); });
    connect(rail, &Rail::loadingFinished, this, [this](quint32 token, const QString &reason, quint32 id) { Q_EMIT LoadingFinished(token, reason, id); });
    connect(client, &Client::connected, this, [this] {
        m_connected = true;
        Q_EMIT Connected();
    });
    connect(client, &Client::finished, this, [this](const QString &error, bool authFailed) {
        m_connected = false;
        Q_EMIT Disconnected(error, authFailed);
    });
}

bool Service::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    if (!bus.registerObject(QStringLiteral("/org/wildos/RemoteApp"), this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        return false;
    }
    // A second session (Windows' whole desktop, say) leaves the name to the
    // first; the agent talks to the one that has it.
    return bus.registerService(QStringLiteral("org.wildos.RemoteApp"));
}

uint Service::Prepare(const QString &appId, const QString &title, const QString &icon, const QString &program, uint delayMs)
{
    return m_rail->prepare(appId, title, icon, program, int(std::min(delayMs, 5000u)));
}

bool Service::Update(uint token, const QString &stage, const QString &reason, int percent, const QString &progress)
{
    return m_rail->update(token, stage, reason, std::clamp(percent, -1, 100), progress);
}

bool Service::Cancel(uint token)
{
    return m_rail->cancel(token, QStringLiteral("cancelled"));
}

void Service::SetWindowApp(qulonglong windowId, const QString &appId)
{
    m_rail->setWindowApp(quint32(windowId), appId);
}

void Service::CloseWindow(qulonglong windowId)
{
    m_client->sysCommand(quint32(windowId), SC_CLOSE);
}

void Service::Disconnect()
{
    m_client->stop();
}

QString Service::Status()
{
    QJsonArray windows;
    for (quint32 id : m_rail->windowIds()) {
        RailWindow *w = m_rail->window(id);
        windows.append(QJsonObject{{QStringLiteral("id"), qint64(id)}, {QStringLiteral("app"), w ? w->appId() : QString()},
                                   {QStringLiteral("visible"), w && w->isVisible()}});
    }
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("connected"), m_connected}, {QStringLiteral("windows"), windows}}).toJson(QJsonDocument::Compact));
}

QString Service::Placements()
{
    return QString::fromUtf8(QJsonDocument(Rail::placements()).toJson(QJsonDocument::Compact));
}

void Service::SavePlacement(const QString &appId, double x, double y)
{
    Rail::savePlacement(appId, QPoint(qRound(x), qRound(y)));
}
