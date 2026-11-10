#include "wayland.h"

#include <QGuiApplication>
#include <QPainter>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-toplevel-icon-v1-client-protocol.h"

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

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
}

void WaylandExtras::global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t)
{
    auto *self = static_cast<WaylandExtras *>(data);
    if (std::strcmp(interface, xdg_toplevel_icon_manager_v1_interface.name) == 0) {
        self->m_icons = static_cast<xdg_toplevel_icon_manager_v1 *>(wl_registry_bind(registry, name, &xdg_toplevel_icon_manager_v1_interface, 1));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    }
}

void WaylandExtras::globalRemove(void *, wl_registry *, uint32_t)
{
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
