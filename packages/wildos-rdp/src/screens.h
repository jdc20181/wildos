#pragma once

#include "monitors.h"

#include <QObject>
#include <QTimer>

class QScreen;

// This desktop's screens as monitors for Windows, kept up to date as they
// are plugged in, taken away, moved about or rescaled.
class Screens : public QObject
{
    Q_OBJECT

public:
    explicit Screens(QObject *parent = nullptr);

    // Windows' desktop for the screens now, the primary first. Each screen
    // has its own scale when KWin says exactly what it is; otherwise (or with
    // WILDOS_RDP_MONITORS=uniform) every screen is at `fallback`, the one
    // scale Windows draws everything at, which keeps every window the size
    // it should be at some cost in sharpness.
    QList<Monitor> layout(double fallback) const;

Q_SIGNALS:
    // A moment after the screens last changed; a monitor being plugged in
    // changes several things one after another.
    void changed();

private:
    void watch(QScreen *screen);

    QTimer m_settle;
};
