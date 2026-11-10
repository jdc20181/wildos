#pragma once

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QString>

class QWindow;
struct wl_buffer;
struct wl_registry;
struct wl_shm;
struct xdg_toplevel_icon_manager_v1;

// What Qt 6.8 does not do for a Wayland window and Windows apps need:
// changing a window's app id while it is on screen (it moves to its app's
// taskbar group without being shown again), and giving it its own icon
// (xdg-toplevel-icon), so a Windows app the desktop has no entry for shows
// the icon Windows sends rather than a generic one.
class WaylandExtras
{
public:
    static WaylandExtras &instance();

    // Both work only while the window is shown; false when the compositor
    // or the window cannot take them.
    bool setAppId(QWindow *window, const QString &appId);
    bool setIcon(QWindow *window, const QImage &icon);

private:
    WaylandExtras();
    void *toplevel(QWindow *window) const;

    static void global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void globalRemove(void *data, wl_registry *registry, uint32_t name);

    wl_registry *m_registry = nullptr;
    wl_shm *m_shm = nullptr;
    xdg_toplevel_icon_manager_v1 *m_icons = nullptr;
    // The buffer behind each window's icon, kept until it is replaced.
    QHash<QWindow *, wl_buffer *> m_buffers;
};
