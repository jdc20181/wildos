#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Thunderbolt and USB4 devices through boltd (org.freedesktop.bolt on the
// system bus), which is what KDE's Thunderbolt module drives. Allowing a
// device enrols it with boltd, so it is let in again on its own the next
// time; forgetting takes that away. boltd asks polkit, which lets an
// administrator in and asks anyone else for a password.
class Thunderbolt : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool hasPorts READ hasPorts NOTIFY changed)
    Q_PROPERTY(bool firmwareOff READ firmwareOff NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)

public:
    explicit Thunderbolt(QObject *parent = nullptr);

    bool available() const { return m_available; }
    bool hasPorts() const { return m_hasPorts; }
    // The firmware leaves the ports to DisplayPort or USB only.
    bool firmwareOff() const { return m_firmwareOff; }
    // Peripherals as { uid, name, vendor, status, stored }, where status is
    // boltd's own word: connected, authorizing, authorized, auth-error,
    // disconnected and so on.
    QVariantList devices() const { return m_devices; }
    // The uid of the device being allowed or forgotten.
    QString busy() const { return m_busy; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void allow(const QString &uid);
    Q_INVOKABLE void forget(const QString &uid);

Q_SIGNALS:
    void changed();
    void busyChanged();
    void failed(const QString &title, const QString &reason);

private Q_SLOTS:
    void onChanged();

private:
    void call(const QString &uid, const QString &method, const QVariantList &arguments, const QString &failure);
    void setBusy(const QString &uid);

    bool m_available = false;
    bool m_hasPorts = false;
    bool m_firmwareOff = false;
    QVariantList m_devices;
    QString m_busy;
};
