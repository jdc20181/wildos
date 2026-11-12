#include "input.h"
#include "client.h"
#include "keymap.h"

#include <QKeyEvent>

#include <freerdp/input.h>

#include <algorithm>

namespace rdpinput
{
void button(Client *client, Qt::MouseButton button, bool down, QPoint at)
{
    const quint16 press = down ? PTR_FLAGS_DOWN : 0;
    switch (button) {
    case Qt::LeftButton: client->sendMouse(PTR_FLAGS_BUTTON1 | press, at.x(), at.y()); break;
    case Qt::RightButton: client->sendMouse(PTR_FLAGS_BUTTON2 | press, at.x(), at.y()); break;
    case Qt::MiddleButton: client->sendMouse(PTR_FLAGS_BUTTON3 | press, at.x(), at.y()); break;
    case Qt::BackButton: client->sendExtendedMouse(PTR_XFLAGS_BUTTON1 | (down ? PTR_XFLAGS_DOWN : 0), at.x(), at.y()); break;
    case Qt::ForwardButton: client->sendExtendedMouse(PTR_XFLAGS_BUTTON2 | (down ? PTR_XFLAGS_DOWN : 0), at.x(), at.y()); break;
    default: break;
    }
}

void move(Client *client, QPoint at)
{
    client->sendMouse(PTR_FLAGS_MOVE, at.x(), at.y());
}

void wheel(Client *client, QPoint angleDelta, QPoint at, int &pendingX, int &pendingY)
{
    pendingY += angleDelta.y();
    pendingX -= angleDelta.x();
    auto send = [&](int &pending, quint16 axis) {
        while (pending != 0) {
            const int step = std::clamp(pending, -255, 255);
            pending -= step;
            const quint16 flags = axis | quint16(step < 0 ? (PTR_FLAGS_WHEEL_NEGATIVE | ((step + 256) & 0xff)) : step);
            client->sendWheel(flags, at.x(), at.y());
        }
    };
    send(pendingY, PTR_FLAGS_WHEEL);
    send(pendingX, PTR_FLAGS_HWHEEL);
}

void pinch(Client *client, Qt::NativeGestureType type, qreal value, QPoint at, int &pending)
{
    constexpr quint32 leftCtrl = 0x1D;
    switch (type) {
    case Qt::BeginNativeGesture:
        pending = 0;
        client->sendKey(true, false, leftCtrl);
        break;
    case Qt::ZoomNativeGesture: {
        pending += qRound(value * 1200);
        int ignored = 0;
        // Whole notches only, so the app sees the same steps a mouse
        // wheel would give it.
        const int notches = pending / 120 * 120;
        pending -= notches;
        wheel(client, QPoint(0, notches), at, ignored, ignored);
        break;
    }
    case Qt::EndNativeGesture:
        client->sendKey(false, false, leftCtrl);
        break;
    default:
        break;
    }
}

void key(Client *client, const QKeyEvent *event, bool press, QList<quint32> &down)
{
    const quint32 native = event->nativeScanCode();
    const quint32 evdev = native > 8 ? native - 8 : 0;
    if (press) {
        if (evdev == 119) {
            client->sendPause();
            return;
        }
        if (const quint32 scancode = keymap::rdpScancode(evdev)) {
            client->sendKey(true, event->isAutoRepeat(), scancode);
            if (!down.contains(scancode)) {
                down.append(scancode);
            }
            return;
        }
        for (const QChar c : event->text()) {
            client->sendUnicode(true, c.unicode());
            client->sendUnicode(false, c.unicode());
        }
        return;
    }
    if (event->isAutoRepeat()) {
        return;
    }
    if (const quint32 scancode = keymap::rdpScancode(evdev)) {
        client->sendKey(false, false, scancode);
        down.removeAll(scancode);
    }
}

void releaseAll(Client *client, QList<quint32> &down)
{
    for (const quint32 scancode : std::as_const(down)) {
        client->sendKey(false, false, scancode);
    }
    down.clear();
}

void chord(Client *client, const QList<quint32> &scancodes)
{
    for (const quint32 s : scancodes) {
        client->sendKey(true, false, s);
    }
    for (auto it = scancodes.crbegin(); it != scancodes.crend(); ++it) {
        client->sendKey(false, false, *it);
    }
}
}
