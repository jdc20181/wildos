#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Mice and touchpads as KWin sees them, over its InputDevice D-Bus
// interface, which is what the KDE mouse and touchpad modules use on
// Wayland. KWin applies a change at once and saves it for that device.
class InputDevices : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList mice READ mice NOTIFY changed)
    Q_PROPERTY(QVariantList touchpads READ touchpads NOTIFY changed)

public:
    explicit InputDevices(QObject *parent = nullptr);

    QVariantList mice() const { return m_mice; }
    QVariantList touchpads() const { return m_touchpads; }

    Q_INVOKABLE void reload();
    // Sets one property, such as "leftHanded" or "pointerAcceleration", on
    // every device of the kind ("mouse" or "touchpad"), as Windows has one
    // setting for all mice.
    Q_INVOKABLE void set(const QString &kind, const QString &property, const QVariant &value);

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onDevicesChanged();

private:
    QVariantList m_mice;
    QVariantList m_touchpads;
};
