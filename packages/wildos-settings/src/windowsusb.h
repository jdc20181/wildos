#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Windows apps > Windows mode > USB devices: the devices plugged in, and
// which of them Windows gets. The Windows mode system service
// (org.wildos.WindowsMode1 on the system bus, winmode/usb.py in
// wildos-winmode) decides and remembers; this only shows its list and
// passes on the switch. Allowing a device USB protection blocked goes
// through the same helper as the shield's "Allow once".
class WindowsUsb : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // { port, name, kind, use, state, problem } per device, where state is
    // windows, waiting, wildos, blocked or refused, and problem says why a
    // device stays with Wild OS (keyboard, pointer, system, in-use, blocked)
    // or why Windows didn't take it (full, failed).
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    // Windows is running or paused, so a remembered device is with it now.
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    // The port of the device whose switch or Allow once is under way.
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canAllow READ canAllow CONSTANT)

public:
    explicit WindowsUsb(QObject *parent = nullptr);

    QVariantList devices() const { return m_devices; }
    bool running() const { return m_running; }
    bool loaded() const { return m_loaded; }
    QString busy() const { return m_busy; }
    bool canAllow() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setUse(const QString &port, bool use);
    Q_INVOKABLE void allow(const QString &port);

Q_SIGNALS:
    void changed();
    void busyChanged();
    void failed(const QString &message);

private:
    void setBusy(const QString &port);

    bool m_installed = false;
    QVariantList m_devices;
    bool m_running = false;
    bool m_loaded = false;
    QString m_busy;
};
