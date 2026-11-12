#pragma once

#include <QObject>
#include <QQmlEngine>

// Requests from outside plasmashell for the Wild OS plasmoids: KWin's hot
// corners and touchpad gestures open Start's app list or the notifications
// card through it. plasmashell owns org.kde.plasmashell, and this registers
// on that connection:
//
//   service    org.kde.plasmashell
//   path       /org/wildos/Shell
//   interface  org.wildos.Shell
//   ToggleAllApps(int x, int y)        Start at All apps, or closes Start
//   ToggleNotifications(int x, int y)  the clock's notifications card, or
//                                      closes it
//
// x and y are a point in global (logical) coordinates; only the taskbar on
// the screen holding that point answers, so each screen's Start and clock do
// not all open at once. Other sessions call these methods; keep them stable.
class ShellRequests : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ShellRequests(QObject *parent = nullptr);

Q_SIGNALS:
    void allAppsRequested(int x, int y);
    void notificationsRequested(int x, int y);
};
