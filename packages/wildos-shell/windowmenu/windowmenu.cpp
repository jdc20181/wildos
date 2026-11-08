/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowmenu.h"

#include <core/output.h>
#include <input_event.h>
#include <main.h>
#include <options.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <QAction>
#include <QDBusConnection>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>

namespace WildOS
{

namespace
{
const QString service = QStringLiteral("org.wildos.WindowMenu");
const QString shortcutName = QStringLiteral("Window Operations Menu");
}

WindowMenu::WindowMenu()
    : KWin::Plugin()
    // After KWin's pop-up filter, so a press that closes an open pop-up is
    // not also taken as a title bar click, and before the decorations'.
    , KWin::InputEventFilter(KWin::InputFilterOrder::Popup)
{
    KWin::input()->installInputEventFilter(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerObject(QStringLiteral("/WindowMenu"), this, QDBusConnection::ExportScriptableSlots);
    bus.registerService(service);
    // The workspace makes its shortcuts as it starts; plugins may load first.
    QTimer::singleShot(0, this, &WindowMenu::takeOverShortcut);
}

WindowMenu::~WindowMenu()
{
    QDBusConnection::sessionBus().unregisterService(service);
    if (KWin::workspace()) {
        if (QAction *action = KWin::workspace()->findChild<QAction *>(shortcutName)) {
            disconnect(action, &QAction::triggered, this, nullptr);
            connect(action, &QAction::triggered, KWin::workspace(), &KWin::Workspace::slotWindowOperations);
        }
    }
}

// KWin's "Window Operations Menu" shortcut keeps its name, its place in
// Settings and whatever keys the person gave it; only what it opens
// changes.
void WindowMenu::takeOverShortcut()
{
    QAction *action = KWin::workspace()->findChild<QAction *>(shortcutName);
    if (!action) {
        return;
    }
    disconnect(action, &QAction::triggered, KWin::workspace(), nullptr);
    connect(action, &QAction::triggered, this, &WindowMenu::showForActive);
}

bool WindowMenu::pointerButton(KWin::PointerButtonEvent *event)
{
    if (event->button != Qt::RightButton) {
        return false;
    }
    if (event->state == KWin::PointerButtonState::Released) {
        const bool swallow = m_swallowRelease;
        m_swallowRelease = false;
        return swallow;
    }
    if (m_view && m_view->isVisible()) {
        return false;
    }
    KWin::Window *window = KWin::input()->findToplevel(event->position);
    if (!window || window->isInternal() || !window->isDecorated()) {
        return false;
    }
    const QPointF at = event->position;
    if (!window->frameGeometry().contains(at) || at.y() >= window->clientGeometry().top()) {
        return false;
    }
    m_swallowRelease = true;
    KWin::workspace()->activateWindow(window);
    KWin::workspace()->raiseWindow(window);
    show(window, at);
    return true;
}

void WindowMenu::ShowAtPointer()
{
    const QPointF at = KWin::input()->globalPointer();
    KWin::Window *window = KWin::input()->findToplevel(at);
    if (!window || window->isInternal()) {
        window = KWin::workspace()->activeWindow();
    }
    show(window, at);
}

void WindowMenu::showForActive()
{
    KWin::Window *window = KWin::workspace()->activeWindow();
    if (!window) {
        return;
    }
    // Under the left end of the title bar, where Windows opens it for
    // Alt+Space.
    const QRectF frame = window->frameGeometry();
    show(window, QPointF(frame.left() + 8, window->isDecorated() ? window->clientGeometry().top() : frame.top() + 8));
}

QVariantMap WindowMenu::describe(KWin::Window *window) const
{
    QVariantMap state;
    state[QStringLiteral("canMinimise")] = window->isMinimizable();
    state[QStringLiteral("canMaximise")] = window->isMaximizable();
    state[QStringLiteral("maximised")] = window->maximizeMode() == KWin::MaximizeFull;
    state[QStringLiteral("canSnap")] = window->isMaximizable() && window->isMovable() && !window->isFullScreen();
    state[QStringLiteral("keepAbove")] = window->keepAbove();
    state[QStringLiteral("canMove")] = window->isMovable() && !window->isFullScreen();
    state[QStringLiteral("canClose")] = window->isCloseable();

    QVariantList desktops;
    const QList<KWin::VirtualDesktop *> mine = window->desktops();
    const bool everywhere = window->isOnAllDesktops();
    for (KWin::VirtualDesktop *desktop : KWin::VirtualDesktopManager::self()->desktops()) {
        desktops.append(QVariantMap{
            {QStringLiteral("name"), desktop->name()},
            {QStringLiteral("current"), !everywhere && mine.contains(desktop)},
        });
    }
    state[QStringLiteral("desktops")] = desktops;
    state[QStringLiteral("canChangeDesktop")] = !window->isSpecialWindow();
    return state;
}

bool WindowMenu::ensureView()
{
    if (m_view) {
        return true;
    }
    m_engine = std::make_unique<QQmlEngine>();
    QQmlComponent component(m_engine.get(), QUrl(QStringLiteral("qrc:/wildos/windowmenu/WindowMenu.qml")));
    std::unique_ptr<QObject> object(component.create());
    auto view = qobject_cast<QQuickWindow *>(object.get());
    if (!view) {
        qWarning() << "Wild OS window menu:" << component.errors();
        m_engine.reset();
        return false;
    }
    object.release();
    m_view.reset(view);
    m_view->setProperty("controller", QVariant::fromValue<QObject *>(this));
    return true;
}

void WindowMenu::show(KWin::Window *window, const QPointF &position)
{
    if (!window || window->isDesktop() || window->isDock() || !ensureView()) {
        return;
    }
    KWin::Output *output = KWin::workspace()->outputAt(position);
    if (!output) {
        return;
    }
    m_window = window;
    const QRect area = output->geometry();
    m_view->setGeometry(area);
    QMetaObject::invokeMethod(m_view.get(), "openMenu",
                              Q_ARG(QVariant, describe(window)),
                              Q_ARG(QVariant, position.x() - area.x()),
                              Q_ARG(QVariant, position.y() - area.y()));
}

void WindowMenu::act(const QString &action, const QVariant &argument)
{
    // As KWin's own menu does, the operation runs once the menu has gone,
    // so a move starts with the pointer free.
    QTimer::singleShot(0, this, [window = m_window, action, argument] {
        if (!window) {
            return;
        }
        KWin::Workspace *workspace = KWin::workspace();
        if (action == QLatin1String("minimise")) {
            workspace->performWindowOperation(window, KWin::Options::MinimizeOp);
        } else if (action == QLatin1String("maximise")) {
            workspace->performWindowOperation(window, KWin::Options::MaximizeOp);
        } else if (action == QLatin1String("keepAbove")) {
            workspace->performWindowOperation(window, KWin::Options::KeepAboveOp);
        } else if (action == QLatin1String("move")) {
            workspace->performWindowOperation(window, KWin::Options::UnrestrictedMoveOp);
        } else if (action == QLatin1String("close")) {
            workspace->performWindowOperation(window, KWin::Options::CloseOp);
        } else if (action == QLatin1String("snap")) {
            const QString where = argument.toString();
            KWin::QuickTileMode mode;
            if (where.contains(QLatin1String("left"))) {
                mode |= KWin::QuickTileFlag::Left;
            }
            if (where.contains(QLatin1String("right"))) {
                mode |= KWin::QuickTileFlag::Right;
            }
            if (where.contains(QLatin1String("top"))) {
                mode |= KWin::QuickTileFlag::Top;
            }
            if (where.contains(QLatin1String("bottom"))) {
                mode |= KWin::QuickTileFlag::Bottom;
            }
            if (mode != KWin::QuickTileMode(KWin::QuickTileFlag::None)) {
                window->setQuickTileModeAtCurrentPosition(mode);
            }
        } else if (action == QLatin1String("desktop")) {
            KWin::VirtualDesktopManager *manager = KWin::VirtualDesktopManager::self();
            const int index = argument.toInt();
            KWin::VirtualDesktop *desktop = nullptr;
            if (index < 0) {
                desktop = manager->createVirtualDesktop(manager->count());
            } else if (index < manager->desktops().size()) {
                desktop = manager->desktops().at(index);
            }
            if (desktop) {
                workspace->sendWindowToDesktops(window, {desktop}, false);
            }
        }
    });
}

}
