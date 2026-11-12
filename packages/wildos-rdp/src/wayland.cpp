#include "wayland.h"

#include <QGuiApplication>
#include <QPainter>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "kde-output-device-v2-client-protocol.h"
#include "xdg-toplevel-icon-v1-client-protocol.h"

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
using Output = WaylandExtras::Output;

// Version 2 is the first with the screen's name, which ties it to a QScreen;
// nothing later is needed, and a version bound is a promise to handle every
// event it has.
constexpr uint32_t outputVersion = 2;

void modeSize(void *data, kde_output_device_mode_v2 *mode, int32_t width, int32_t height)
{
    static_cast<Output *>(data)->modes.insert(mode, QSize(width, height));
}

void modeRefresh(void *, kde_output_device_mode_v2 *, int32_t)
{
}

void modePreferred(void *, kde_output_device_mode_v2 *)
{
}

void modeRemoved(void *data, kde_output_device_mode_v2 *mode)
{
    auto *o = static_cast<Output *>(data);
    o->modes.remove(mode);
    if (o->current == mode) {
        o->current = nullptr;
    }
    kde_output_device_mode_v2_destroy(mode);
}

const kde_output_device_mode_v2_listener modeListener = {
    .size = modeSize,
    .refresh = modeRefresh,
    .preferred = modePreferred,
    .removed = modeRemoved,
};

void geometry(void *data, kde_output_device_v2 *, int32_t, int32_t, int32_t, int32_t, int32_t, const char *, const char *, int32_t transform)
{
    static_cast<Output *>(data)->transform = transform;
}

void currentMode(void *data, kde_output_device_v2 *, kde_output_device_mode_v2 *mode)
{
    static_cast<Output *>(data)->current = mode;
}

void mode(void *data, kde_output_device_v2 *, kde_output_device_mode_v2 *mode)
{
    kde_output_device_mode_v2_add_listener(mode, &modeListener, data);
}

void done(void *data, kde_output_device_v2 *)
{
    auto *o = static_cast<Output *>(data);
    QSize pixels = o->modes.value(o->current);
    // Turned a quarter, the screen is as tall as its mode is wide.
    if (o->transform % 2 == 1) {
        pixels.transpose();
    }
    o->info.pixels = pixels;
    o->extras->outputChanged();
}

void scale(void *data, kde_output_device_v2 *, wl_fixed_t factor)
{
    static_cast<Output *>(data)->info.scale = wl_fixed_to_double(factor);
}

void ignoreString(void *, kde_output_device_v2 *, const char *)
{
}

void ignoreInt(void *, kde_output_device_v2 *, int32_t)
{
}

void ignoreUint(void *, kde_output_device_v2 *, uint32_t)
{
}

void enabled(void *data, kde_output_device_v2 *, int32_t on)
{
    static_cast<Output *>(data)->info.enabled = on != 0;
}

void name(void *data, kde_output_device_v2 *, const char *name)
{
    static_cast<Output *>(data)->info.name = QString::fromUtf8(name);
}

const kde_output_device_v2_listener outputListener = {
    .geometry = geometry,
    .current_mode = currentMode,
    .mode = mode,
    .done = done,
    .scale = scale,
    .edid = ignoreString,
    .enabled = enabled,
    .uuid = ignoreString,
    .serial_number = ignoreString,
    .eisa_id = ignoreString,
    .capabilities = ignoreUint,
    .overscan = ignoreUint,
    .vrr_policy = ignoreUint,
    .rgb_range = ignoreUint,
    .name = name,
};
}

WaylandExtras &WaylandExtras::instance()
{
    static WaylandExtras extras;
    return extras;
}

WaylandExtras::WaylandExtras()
{
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!app || !app->display()) {
        return;
    }
    static const wl_registry_listener listener = {global, globalRemove};
    m_registry = wl_display_get_registry(app->display());
    wl_registry_add_listener(m_registry, &listener, this);
    wl_display_roundtrip(app->display());
    if (!m_outputs.isEmpty()) {
        // The screens describe themselves once they are bound.
        wl_display_roundtrip(app->display());
    }
}

QList<OutputInfo> WaylandExtras::outputs() const
{
    QList<OutputInfo> out;
    for (const Output *o : m_outputs) {
        if (o->info.enabled && !o->info.name.isEmpty() && o->info.scale > 0) {
            out.append(o->info);
        }
    }
    return out;
}

void WaylandExtras::outputChanged()
{
    if (m_outputsChanged) {
        m_outputsChanged();
    }
}

void WaylandExtras::global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandExtras *>(data);
    if (std::strcmp(interface, kde_output_device_v2_interface.name) == 0 && version >= outputVersion) {
        auto *o = new Output;
        o->extras = self;
        o->global = name;
        o->device = static_cast<kde_output_device_v2 *>(wl_registry_bind(registry, name, &kde_output_device_v2_interface, outputVersion));
        kde_output_device_v2_add_listener(o->device, &outputListener, o);
        self->m_outputs.append(o);
    } else if (std::strcmp(interface, xdg_toplevel_icon_manager_v1_interface.name) == 0) {
        self->m_icons = static_cast<xdg_toplevel_icon_manager_v1 *>(wl_registry_bind(registry, name, &xdg_toplevel_icon_manager_v1_interface, 1));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    }
}

void WaylandExtras::globalRemove(void *data, wl_registry *, uint32_t name)
{
    auto *self = static_cast<WaylandExtras *>(data);
    for (qsizetype i = 0; i < self->m_outputs.size(); ++i) {
        Output *o = self->m_outputs[i];
        if (o->global != name) {
            continue;
        }
        // A screen unplugged.
        self->m_outputs.removeAt(i);
        for (auto it = o->modes.cbegin(); it != o->modes.cend(); ++it) {
            kde_output_device_mode_v2_destroy(it.key());
        }
        kde_output_device_v2_destroy(o->device);
        delete o;
        self->outputChanged();
        return;
    }
}

void *WaylandExtras::toplevel(QWindow *window) const
{
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native || !window || !window->handle()) {
        return nullptr;
    }
    return native->nativeResourceForWindow(QByteArrayLiteral("xdg_toplevel"), window);
}

bool WaylandExtras::setAppId(QWindow *window, const QString &appId)
{
    auto *top = static_cast<xdg_toplevel *>(toplevel(window));
    if (!top) {
        return false;
    }
    xdg_toplevel_set_app_id(top, appId.toUtf8().constData());
    return true;
}

bool WaylandExtras::setIcon(QWindow *window, const QImage &icon)
{
    auto *top = static_cast<xdg_toplevel *>(toplevel(window));
    if (!top || !m_icons || !m_shm || icon.isNull()) {
        return false;
    }
    // The protocol takes square pictures only: the icon is centred in one.
    const int side = std::max(icon.width(), icon.height());
    QImage square(side, side, QImage::Format_ARGB32_Premultiplied);
    square.fill(Qt::transparent);
    {
        QPainter p(&square);
        p.drawImage(QPoint((side - icon.width()) / 2, (side - icon.height()) / 2), icon);
    }
    const int stride = int(square.bytesPerLine());
    const size_t bytes = size_t(stride) * side;
    const int fd = memfd_create("wildos-rdp-icon", MFD_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    if (ftruncate(fd, off_t(bytes)) != 0) {
        close(fd);
        return false;
    }
    void *map = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return false;
    }
    std::memcpy(map, square.constBits(), bytes);
    munmap(map, bytes);
    wl_shm_pool *pool = wl_shm_create_pool(m_shm, fd, int32_t(bytes));
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, side, side, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    xdg_toplevel_icon_v1 *handle = xdg_toplevel_icon_manager_v1_create_icon(m_icons);
    xdg_toplevel_icon_v1_add_buffer(handle, buffer, 1);
    xdg_toplevel_icon_manager_v1_set_icon(m_icons, top, handle);
    xdg_toplevel_icon_v1_destroy(handle);

    if (wl_buffer *old = m_buffers.take(window)) {
        wl_buffer_destroy(old);
    } else {
        QObject::connect(window, &QObject::destroyed, window, [this, window] {
            if (wl_buffer *b = m_buffers.take(window)) {
                wl_buffer_destroy(b);
            }
        });
    }
    m_buffers.insert(window, buffer);
    return true;
}
