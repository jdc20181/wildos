/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <input.h>
#include <plugin.h>

#include <QPointer>
#include <QVariantMap>

#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace KWin
{
class Window;
}

namespace WildOS
{

// The window menu of MenusShell.dc ("Window title bar"): Minimise,
// Maximise, Snap, Move to desktop, Keep on top, Move and Close, drawn with
// the shared Wild OS menu instead of KWin's widget menu. It opens on a
// right click on a title bar KWin draws, on the shortcut KWin calls
// "Window Operations Menu" (Alt+Space in Wild OS), and over D-Bus
// (org.wildos.WindowMenu ShowAtPointer) for the apps that draw their own
// title bar with WildOS.UI.
class WindowMenu : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.WindowMenu")

public:
    WindowMenu();
    ~WindowMenu() override;

    bool pointerButton(KWin::PointerButtonEvent *event) override;

    // Called by WindowMenu.qml when a row is chosen.
    Q_INVOKABLE void act(const QString &action, const QVariant &argument = {});

public Q_SLOTS:
    Q_SCRIPTABLE void ShowAtPointer();

private:
    void takeOverShortcut();
    void showForActive();
    void show(KWin::Window *window, const QPointF &position);
    QVariantMap describe(KWin::Window *window) const;
    bool ensureView();

    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_view;
    QPointer<KWin::Window> m_window;
    bool m_swallowRelease = false;
};

}
