#include "videooutput.h"
#include "player.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickOpenGLUtils>
#include <QtGui/qguiapplication_platform.h>

namespace
{
void *procAddress(void *, const char *name)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(QByteArray(name))) : nullptr;
}

// Mesa's llvmpipe and the like draw on the processor.
bool softwareRenderer()
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        return false;
    }
    const auto *name = reinterpret_cast<const char *>(context->functions()->glGetString(GL_RENDERER));
    const QByteArray renderer = QByteArray(name ? name : "").toLower();
    return renderer.contains("llvmpipe") || renderer.contains("softpipe") || renderer.contains("swiftshader") || renderer.contains("software");
}

class VideoRenderer : public QQuickFramebufferObject::Renderer
{
public:
    VideoRenderer() = default;

    // On the render thread, with the window's GL context current, so the
    // render context is freed on the thread and context it was made in,
    // and before the core (Player outlives every window).
    ~VideoRenderer() override
    {
        if (m_context) {
            mpv_render_context_free(m_context);
        }
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        if (!m_context) {
            makeContext();
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        if (!m_context) {
            return;
        }
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo target{static_cast<int>(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flip = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        // Qt Quick and mpv share the window's GL context, and each would
        // otherwise draw on the state the other left behind.
        QQuickOpenGLUtils::resetOpenGLState();
        mpv_render_context_render(m_context, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    void makeContext()
    {
        Player *player = Player::instance();
        // On a software renderer (the VM, a PC without a working GPU
        // driver) mpv's full chain of shader passes is slow, and on Mesa's
        // llvmpipe it now and then draws every frame black from the start.
        // Its simple path scales with plain bilinear filtering instead.
        if (softwareRenderer()) {
            mpv_set_property_string(player->handle(), "gpu-dumb-mode", "yes");
        }
        mpv_opengl_init_params gl{&procAddress, nullptr};
        QList<mpv_render_param> params{
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
        };
        // Hardware decoding hands frames over through the display
        // connection; without it mpv decodes on the processor.
#if QT_CONFIG(wayland)
        if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            params.append({MPV_RENDER_PARAM_WL_DISPLAY, wayland->display()});
        }
#endif
#if QT_CONFIG(xcb)
        if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
            params.append({MPV_RENDER_PARAM_X11_DISPLAY, x11->display()});
        }
#endif
        params.append({MPV_RENDER_PARAM_INVALID, nullptr});
        if (mpv_render_context_create(&m_context, player->handle(), params.data()) < 0) {
            qWarning("Videos could not draw video with OpenGL.");
            m_context = nullptr;
            return;
        }
        mpv_render_context_set_update_callback(m_context, &Player::frameWaiting, player);
        // Files wait for this: mpv picks its video output as a file starts.
        QMetaObject::invokeMethod(player, "setRenderReady", Qt::QueuedConnection);
    }

    mpv_render_context *m_context = nullptr;
};
}

VideoOutput::VideoOutput(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    connect(Player::instance(), &Player::frameReady, this, &QQuickItem::update);
}

QQuickFramebufferObject::Renderer *VideoOutput::createRenderer() const
{
    return new VideoRenderer;
}
