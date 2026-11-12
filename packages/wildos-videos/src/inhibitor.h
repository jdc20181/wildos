#pragma once

#include <QObject>

// Keeps the screen on and the PC awake while a video plays, and lets them
// go again on a pause: org.freedesktop.ScreenSaver for locking and the
// screen dimming, org.freedesktop.PowerManagement for sleep. Both are let
// go by the session if Videos quits or crashes, since they belong to its
// D-Bus connection.
class Inhibitor : public QObject
{
    Q_OBJECT

public:
    explicit Inhibitor(QObject *parent);
    ~Inhibitor() override;

    void setActive(bool active);

private:
    struct Hold {
        QString service;
        QString path;
        QString interface;
        uint cookie = 0;
        bool waiting = false;
    };

    void take(Hold &hold);
    void release(Hold &hold);

    bool m_active = false;
    Hold m_screen;
    Hold m_sleep;
};
