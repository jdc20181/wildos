#pragma once

#include <QList>
#include <QPoint>
#include <Qt>

class Client;
class QKeyEvent;

// The keyboard and pointer as RDP takes them, for every window the engine
// draws into: a Windows mode app's window and a whole remote desktop.
// Points are in the remote desktop's pixels.
namespace rdpinput
{
void button(Client *client, Qt::MouseButton button, bool down, QPoint at);
void move(Client *client, QPoint at);

// Wheels and touchpads alike: Windows takes 120 units a notch and finer
// steps for smooth scrolling. What doesn't fit in one message waits in
// `pendingX` and `pendingY` for the next.
void wheel(Client *client, QPoint angleDelta, QPoint at, int &pendingX, int &pendingY);

// A key, by the scancode of the physical key behind it, or as the character
// it typed when there is no key (an input method, the on-screen keyboard).
// Keys sent down are kept in `down` until they are sent up.
void key(Client *client, const QKeyEvent *event, bool press, QList<quint32> &down);
// Every key still down goes up: the window lost the keyboard, and Windows
// would otherwise keep Alt (from Alt+Tab) held.
void releaseAll(Client *client, QList<quint32> &down);

// A chord of scancodes pressed in order and let go in reverse, for keys the
// desktop keeps for itself (Ctrl+Alt+Del, the Windows key).
void chord(Client *client, const QList<quint32> &scancodes);

// Scancodes for the chords Remote's Keys menu sends.
constexpr quint32 ctrl = 0x1D;
constexpr quint32 alt = 0x38;
constexpr quint32 shift = 0x2A;
constexpr quint32 del = 0x53 | 0x100;
constexpr quint32 tab = 0x0F;
constexpr quint32 windows = 0x5B | 0x100;
constexpr quint32 printScreen = 0x37 | 0x100;
}
