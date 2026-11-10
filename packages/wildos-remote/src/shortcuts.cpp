#include "shortcuts.h"

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"

#include <cstring>

namespace
{
zwp_keyboard_shortcuts_inhibit_manager_v1 *s_manager = nullptr;
bool s_looked = false;

void global(void *, wl_registry *registry, uint32_t name, const char *interface, uint32_t)
{
    if (std::strcmp(interface, zwp_keyboard_shortcuts_inhibit_manager_v1_interface.name) == 0) {
        s_manager = static_cast<zwp_keyboard_shortcuts_inhibit_manager_v1 *>(
            wl_registry_bind(registry, name, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1));
    }
}

void globalRemove(void *, wl_registry *, uint32_t)
{
}
}

ShortcutInhibitor::ShortcutInhibitor(QObject *parent)
    : QObject(parent)
{
}

ShortcutInhibitor::~ShortcutInhibitor()
{
    release();
}

zwp_keyboard_shortcuts_inhibit_manager_v1 *ShortcutInhibitor::manager()
{
    if (!s_looked) {
        s_looked = true;
        auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (app && app->display()) {
            static const wl_registry_listener listener = {global, globalRemove};
            wl_registry *registry = wl_display_get_registry(app->display());
            wl_registry_add_listener(registry, &listener, nullptr);
            wl_display_roundtrip(app->display());
        }
    }
    return s_manager;
}

bool ShortcutInhibitor::available() const
{
    return manager() != nullptr;
}

void ShortcutInhibitor::setWindow(QQuickWindow *window)
{
    if (window == m_window) {
        return;
    }
    release();
    if (m_window) {
        m_window->disconnect(this);
    }
    m_window = window;
    if (window) {
        connect(window, &QWindow::activeChanged, this, &ShortcutInhibitor::apply);
        connect(window, &QWindow::visibleChanged, this, &ShortcutInhibitor::apply);
    }
    apply();
    Q_EMIT changed();
}

void ShortcutInhibitor::setEnabled(bool enabled)
{
    if (enabled != m_enabled) {
        m_enabled = enabled;
        apply();
        Q_EMIT changed();
    }
}

void ShortcutInhibitor::apply()
{
    const bool want = m_enabled && m_window && m_window->isVisible() && m_window->isActive();
    if (!want) {
        release();
        return;
    }
    if (m_inhibitor || !manager()) {
        return;
    }
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    auto *surface = native ? static_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), m_window)) : nullptr;
    wl_seat *seat = app ? app->seat() : nullptr;
    if (!surface || !seat) {
        return;
    }
    m_inhibitor = zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(s_manager, surface, seat);
    m_active = true;
    Q_EMIT activeChanged();
}

void ShortcutInhibitor::release()
{
    if (m_inhibitor) {
        zwp_keyboard_shortcuts_inhibitor_v1_destroy(m_inhibitor);
        m_inhibitor = nullptr;
        if (auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            wl_display_flush(app->display());
        }
    }
    if (m_active) {
        m_active = false;
        Q_EMIT activeChanged();
    }
}
