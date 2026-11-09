#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// AutoPlay, as Plasma's device automounter does it: whether drives and
// memory cards are mounted, ready in Files, when they are plugged in or
// already there at sign-in, and which drives always are. The settings live
// in kded_device_automounterrc, read by the device_automounter module of
// kded, which is loaded or unloaded to match, as KDE's own module does.
class AutoPlay : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool onPlugin READ onPlugin NOTIFY changed)
    Q_PROPERTY(bool onLogin READ onLogin NOTIFY changed)
    Q_PROPERTY(bool unknownDevices READ unknownDevices NOTIFY changed)
    Q_PROPERTY(QVariantList drives READ drives NOTIFY changed)

public:
    explicit AutoPlay(QObject *parent = nullptr);

    bool onPlugin() const { return m_onPlugin; }
    bool onLogin() const { return m_onLogin; }
    bool unknownDevices() const { return m_unknownDevices; }
    // Drives the automounter has seen, as { udi, name, always }.
    QVariantList drives() const { return m_drives; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setOnPlugin(bool on);
    Q_INVOKABLE void setOnLogin(bool on);
    Q_INVOKABLE void setUnknownDevices(bool on);
    // Mount this drive whenever it is plugged in or there at sign-in,
    // whatever the general settings say.
    Q_INVOKABLE void setAlways(const QString &udi, bool always);
    Q_INVOKABLE void forget(const QString &udi);

Q_SIGNALS:
    void changed();

private:
    void save();

    bool m_onPlugin = false;
    bool m_onLogin = false;
    bool m_unknownDevices = false;
    QVariantList m_drives;
};
