#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

struct _SDL_Joystick;
struct _SDL_GameController;

// Game controllers through SDL2, as KDE's game controller module reads
// them, so a controller looks here the way games see it: its name, battery
// and every button and stick live. SDL runs only while the page is open
// (`active`), as it polls the devices.
class GameControllers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY changed)

public:
    explicit GameControllers(QObject *parent = nullptr);
    ~GameControllers() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    // Each controller as { id, name, battery, buttons, axes }: battery is
    // "full", "medium", "low", "empty", "wired" or empty when the
    // controller does not say; buttons are { name, pressed } and axes
    // { name, value, centred }, value from 0 to 1 and centred true for a
    // stick that rests in the middle.
    QVariantList controllers() const { return m_controllers; }

Q_SIGNALS:
    void activeChanged();
    void changed();

private:
    struct Device {
        _SDL_Joystick *joystick = nullptr;
        _SDL_GameController *gamepad = nullptr;
    };

    void start();
    void stop();
    void poll();
    void add(int index);
    void remove(int id);
    void update();

    bool m_active = false;
    bool m_started = false;
    QTimer m_timer;
    QHash<int, Device> m_devices;
    QVariantList m_controllers;
};
