#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QSize>
#include <QString>

#include <functional>

class QWindow;
struct kde_output_device_mode_v2;
struct kde_output_device_v2;
struct wl_buffer;
struct wl_registry;
struct wl_shm;
struct xdg_toplevel_icon_manager_v1;

// A screen as the compositor has it set up. Qt gives a screen's place and
// size in logical units, but on a fractional scale its scale only to a
// window already on it (QScreen rounds 2.25 up to 3); KWin's own output
// description has the exact scale and the screen's mode in pixels.
struct OutputInfo {
    QString name;
    double scale = 0;
    QSize pixels;
    bool enabled = true;
};

// What Qt 6.8 does not do for a Wayland window and Windows apps need:
// changing a window's app id while it is on screen (it moves to its app's
// taskbar group without being shown again), and giving it its own icon
// (xdg-toplevel-icon), so a Windows app the desktop has no entry for shows
// the icon Windows sends rather than a generic one; and each screen's exact
// scale (KWin's kde-output-device-v2).
class WaylandExtras
{
public:
    static WaylandExtras &instance();

    // Both work only while the window is shown; false when the compositor
    // or the window cannot take them.
    bool setAppId(QWindow *window, const QString &appId);
    bool setIcon(QWindow *window, const QImage &icon);

    // Every screen KWin describes; empty under another compositor.
    QList<OutputInfo> outputs() const;
    // Called whenever a screen's description changes.
    void onOutputsChanged(std::function<void()> changed) { m_outputsChanged = std::move(changed); }
    void outputChanged();

    // KWin's output description, one per screen (wayland.cpp).
    struct Output {
        WaylandExtras *extras = nullptr;
        quint32 global = 0;
        kde_output_device_v2 *device = nullptr;
        OutputInfo info;
        int transform = 0;
        kde_output_device_mode_v2 *current = nullptr;
        QHash<kde_output_device_mode_v2 *, QSize> modes;
    };

private:
    WaylandExtras();
    void *toplevel(QWindow *window) const;

    static void global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void globalRemove(void *data, wl_registry *registry, uint32_t name);

    wl_registry *m_registry = nullptr;
    wl_shm *m_shm = nullptr;
    xdg_toplevel_icon_manager_v1 *m_icons = nullptr;
    QList<Output *> m_outputs;
    std::function<void()> m_outputsChanged;
    // The buffer behind each window's icon, kept until it is replaced.
    QHash<QWindow *, wl_buffer *> m_buffers;
};
