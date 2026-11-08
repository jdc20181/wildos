/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "roundcorners.h"

#include <core/output.h>
#include <core/pixelgrid.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <opengl/glplatform.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/openglcontext.h>
#include <window.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFile>
#include <QVector2D>
#include <QVector4D>

Q_LOGGING_CATEGORY(WILDOS_ROUNDCORNERS, "wildos.roundcorners", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(roundcorners);
}

namespace KWin
{

// These mirror packages/wildos-branding/files/aurorae/generate.py (RADIUS,
// the 1px edge and the shadow-window token) and must change with it.
static constexpr qreal s_radius = 14.0;
static constexpr qreal s_edgeWidth = 1.0;
static constexpr qreal s_shadowAlpha = 0.45;
static constexpr qreal s_shadowSigma = 40.0;
static constexpr qreal s_shadowOffsetY = 30.0;
static const QVector4D s_shadowColour(4 / 255.0, 8 / 255.0, 24 / 255.0, s_shadowAlpha);

static const QString s_auroraeLibrary = QStringLiteral("org.kde.kwin.aurorae");
static const QString s_wildosTheme = QStringLiteral("__aurorae__svg__WildOS");

// Our own apps draw their own radius 14 frame and shadow and ask for no
// server side decoration. They are skipped by that alone; the list guards
// against a compositor or rule that decorates them anyway.
static bool isFramelessWildOSApp(EffectWindow *w)
{
    static const QStringList apps = {
        QStringLiteral("org.wildos.files"),
        QStringLiteral("org.wildos.settings"),
        QStringLiteral("org.wildos.store"),
        QStringLiteral("org.wildos.terminal"),
    };
    const Window *window = w->window();
    if (!window) {
        return false;
    }
    return apps.contains(window->resourceClass()) || apps.contains(window->resourceName()) || apps.contains(window->desktopFileName());
}

RoundCornersEffect::RoundCornersEffect()
{
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &RoundCornersEffect::track);
    connect(effects, &EffectsHandler::windowDeleted, this, &RoundCornersEffect::forget);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        track(w);
    }
}

RoundCornersEffect::~RoundCornersEffect()
{
    for (const auto &connections : std::as_const(m_connections)) {
        for (const auto &connection : connections) {
            disconnect(connection);
        }
    }
    if (m_shader) {
        effects->makeOpenGLContextCurrent();
        m_shader.reset();
    }
}

bool RoundCornersEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void RoundCornersEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    readDecorationSetting();
    const auto windows = m_shapes.keys();
    for (EffectWindow *w : windows) {
        update(w);
    }
}

bool RoundCornersEffect::readDecorationSetting()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    config->reparseConfiguration();
    const KConfigGroup group(config, QStringLiteral("org.kde.kdecoration2"));
    // The corners copy this theme's radius, edge and shadow. Any other
    // decoration brings its own shape, so leave its windows alone.
    const bool ours = group.readEntry("library", QString()) == s_auroraeLibrary
        && group.readEntry("theme", QString()) == s_wildosTheme;
    const bool changed = ours != m_ourDecoration;
    m_ourDecoration = ours;
    return changed;
}

void RoundCornersEffect::track(EffectWindow *w)
{
    if (m_connections.contains(w)) {
        return;
    }
    QList<QMetaObject::Connection> connections;
    // Picking another decoration in Settings recreates every window's
    // decoration, which is the first we hear of it.
    connections << connect(w, &EffectWindow::windowDecorationChanged, this, [this](EffectWindow *w) {
        if (readDecorationSetting()) {
            reconfigure(ReconfigureAll);
        } else {
            update(w);
        }
    });
    connections << connect(w, &EffectWindow::windowFullScreenChanged, this, &RoundCornersEffect::update);
    connections << connect(w, &EffectWindow::windowMaximizedStateChanged, this, [this](EffectWindow *w) {
        update(w);
    });
    if (Window *window = w->window()) {
        const auto refresh = [this, w]() {
            update(w);
        };
        connections << connect(window, &Window::tileChanged, this, refresh);
        connections << connect(window, &Window::quickTileModeChanged, this, refresh);
        connections << connect(window, &Window::shadeChanged, this, refresh);
        connections << connect(window, &Window::windowClassChanged, this, refresh);
    }
    m_connections.insert(w, connections);
    m_shapes.insert(w, Shape::Untouched);
    update(w);
}

void RoundCornersEffect::forget(EffectWindow *w)
{
    const auto connections = m_connections.take(w);
    for (const auto &connection : connections) {
        disconnect(connection);
    }
    m_shapes.remove(w);
}

RoundCornersEffect::Shape RoundCornersEffect::shapeFor(EffectWindow *w) const
{
    if (!m_ourDecoration || m_shaderFailed) {
        return Shape::Untouched;
    }
    // Popups, menus, tooltips, the panel, the desktop, OSDs, notifications
    // and the lock screen are none of these.
    if (!w->isNormalWindow() && !w->isDialog()) {
        return Shape::Untouched;
    }
    if (!w->hasDecoration() || isFramelessWildOSApp(w) || w->isFullScreen()) {
        return Shape::Untouched;
    }
    const Window *window = w->window();
    if (!window || window->isShade()) {
        return Shape::Untouched;
    }
    // Aurorae switches to its square title bar on the committed mode, so
    // follow that rather than the requested one or the two would disagree for
    // a frame.
    if (window->maximizeMode() == MaximizeFull) {
        return Shape::Untouched;
    }
    if (window->tile() || window->quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        return Shape::Tiled;
    }
    return Shape::Rounded;
}

void RoundCornersEffect::update(EffectWindow *w)
{
    auto it = m_shapes.find(w);
    if (it == m_shapes.end() || w->isDeleted()) {
        // A closing window keeps the shape it had while its close animation
        // runs.
        return;
    }
    Shape shape = shapeFor(w);
    if (shape != Shape::Untouched && !ensureShader()) {
        shape = Shape::Untouched;
    }
    if (shape == it.value()) {
        return;
    }
    if (shape == Shape::Untouched) {
        unredirect(w);
    } else if (it.value() == Shape::Untouched) {
        redirect(w);
        setShader(w, m_shader.get());
    }
    it.value() = shape;
    w->addRepaintFull();
}

bool RoundCornersEffect::ensureShader()
{
    if (m_shader) {
        return true;
    }
    if (m_shaderFailed) {
        return false;
    }
    ensureResources();
    QFile file(QStringLiteral(":/wildos/roundcorners/roundcorners.frag"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(WILDOS_ROUNDCORNERS) << "Could not read the corner shader";
        m_shaderFailed = true;
        return false;
    }

    // Windows are tracked from signal handlers, outside painting, where the
    // compositor's context is not necessarily current.
    if (!effects->makeOpenGLContextCurrent()) {
        return false;
    }
    // Same choice of dialect as KWin's own generated shaders.
    const auto context = OpenGlContext::currentContext();
    QByteArray header;
    bool modern;
    if (context->isOpenGLES()) {
        modern = context->glslVersion() >= Version(3, 0);
        header = modern ? QByteArrayLiteral("#version 300 es\n") : QByteArray();
        header += QByteArrayLiteral("precision highp float;\n");
    } else {
        modern = context->glslVersion() >= Version(1, 40);
        header = modern ? QByteArrayLiteral("#version 140\n") : QByteArray();
    }
    if (modern) {
        header += QByteArrayLiteral("#define IN in\n#define TEXTURE texture\n#define FRAG_COLOR fragColor\nout vec4 fragColor;\n");
    } else {
        header += QByteArrayLiteral("#define IN varying\n#define TEXTURE texture2D\n#define FRAG_COLOR gl_FragColor\n");
    }

    const ShaderTraits traits = ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation | ShaderTrait::TransformColorspace;
    m_shader = ShaderManager::instance()->generateCustomShader(traits, QByteArray(), header + file.readAll());
    if (!m_shader || !m_shader->isValid()) {
        qCWarning(WILDOS_ROUNDCORNERS) << "The corner shader failed to build";
        m_shader.reset();
        m_shaderFailed = true;
        return false;
    }

    m_expandedOriginLocation = m_shader->uniformLocation("expandedOrigin");
    m_expandedSizeLocation = m_shader->uniformLocation("expandedSize");
    m_frameSizeLocation = m_shader->uniformLocation("frameSize");
    m_radiusLocation = m_shader->uniformLocation("radius");
    m_edgeWidthLocation = m_shader->uniformLocation("edgeWidth");
    m_devicePixelLocation = m_shader->uniformLocation("devicePixel");
    m_roundBottomLocation = m_shader->uniformLocation("roundBottom");
    m_squareTopLocation = m_shader->uniformLocation("squareTop");
    m_shadowColorLocation = m_shader->uniformLocation("shadowColor");
    m_shadowSigmaLocation = m_shader->uniformLocation("shadowSigma");
    m_shadowOffsetYLocation = m_shader->uniformLocation("shadowOffsetY");
    return true;
}

void RoundCornersEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // Without this KWin would skip painting whatever is behind the cut
    // corners, because it takes the client to be opaque there.
    if (m_shapes.value(w, Shape::Untouched) == Shape::Rounded) {
        const QRect frame = w->frameGeometry().toAlignedRect();
        const int r = int(std::ceil(s_radius)) + 1;
        data.opaque -= QRect(frame.left(), frame.bottom() - r + 1, r, r);
        data.opaque -= QRect(frame.right() - r + 1, frame.bottom() - r + 1, r, r);
    }
    effects->prePaintWindow(w, data, presentTime);
}

void RoundCornersEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    const Shape shape = m_shapes.value(w, Shape::Untouched);
    if (shape != Shape::Untouched && m_shader) {
        // The offscreen texture is rendered at the scale of the window's
        // screen, so antialias at that resolution.
        const qreal textureScale = w->screen() ? w->screen()->scale() : viewport.scale();
        const QRectF expanded = snapToPixels(w->expandedGeometry(), textureScale);
        const QRectF frame = snapToPixels(w->frameGeometry(), textureScale);

        ShaderManager::instance()->pushShader(m_shader.get());
        m_shader->setUniform(m_expandedOriginLocation, QVector2D(expanded.topLeft() - frame.topLeft()));
        m_shader->setUniform(m_expandedSizeLocation, QVector2D(expanded.width(), expanded.height()));
        m_shader->setUniform(m_frameSizeLocation, QVector2D(frame.width(), frame.height()));
        m_shader->setUniform(m_radiusLocation, float(s_radius));
        m_shader->setUniform(m_edgeWidthLocation, float(s_edgeWidth));
        m_shader->setUniform(m_devicePixelLocation, float(1.0 / textureScale));
        m_shader->setUniform(m_roundBottomLocation, shape == Shape::Rounded ? 1.0f : 0.0f);
        m_shader->setUniform(m_squareTopLocation, shape == Shape::Tiled ? 1.0f : 0.0f);
        m_shader->setUniform(m_shadowColorLocation, s_shadowColour);
        m_shader->setUniform(m_shadowSigmaLocation, float(s_shadowSigma));
        m_shader->setUniform(m_shadowOffsetYLocation, float(s_shadowOffsetY));
        ShaderManager::instance()->popShader();
    }
    OffscreenEffect::drawWindow(renderTarget, viewport, w, mask, region, data);
}

bool RoundCornersEffect::isActive() const
{
    return m_ourDecoration && !m_shaderFailed;
}

int RoundCornersEffect::requestedEffectChainPosition() const
{
    // After blur and contrast (20, 21), which only read the frame shape, and
    // after the deforming effects such as magic lamp (50), so those deform the
    // already rounded window rather than the other way round.
    return 60;
}

KWIN_EFFECT_FACTORY_SUPPORTED(RoundCornersEffect, "metadata.json", return RoundCornersEffect::supported();)

} // namespace KWin

#include "roundcorners.moc"
