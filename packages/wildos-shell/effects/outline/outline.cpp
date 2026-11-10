/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outline.h"

#include "../focus/focus.h"

#include <core/pixelgrid.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <opengl/glplatform.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
#include <opengl/openglcontext.h>
#include <window.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFile>
#include <QVector2D>

#include <cmath>

Q_LOGGING_CATEGORY(WILDOS_OUTLINE, "wildos.outline", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(outline);
}

namespace KWin
{

// The line round a window not in use, the contrasting edge outside it, and
// the accent ring round the window in use, in logical pixels.
static constexpr qreal s_lineWidth = 1.5;
static constexpr qreal s_edgeWidth = 1.0;
static constexpr qreal s_focusWidth = 4.0;
// The Wild OS title bar and the corner effect round windows to 14
// (packages/wildos-branding/files/aurorae/generate.py); GTK, browsers and
// Windows mode draw their own corners at about 12.
static constexpr qreal s_decoratedRadius = 14.0;
static constexpr qreal s_clientRadius = 12.0;

static QColor readRgb(const KConfigGroup &group, const char *key)
{
    const QStringList rgb = group.readEntry(key, QString()).split(QLatin1Char(','));
    if (rgb.size() < 3) {
        return QColor();
    }
    return QColor(rgb[0].trimmed().toInt(), rgb[1].trimmed().toInt(), rgb[2].trimmed().toInt());
}

static qreal luminance(const QColor &c)
{
    const auto f = [](qreal v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * f(c.redF()) + 0.7152 * f(c.greenF()) + 0.0722 * f(c.blueF());
}

static qreal contrast(const QColor &a, const QColor &b)
{
    const qreal la = luminance(a);
    const qreal lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The accent moved lighter or darker until it stands 3:1 against the
// window colour, the contrast WCAG asks of a focus indicator. Light pastel
// accents such as Glacier would otherwise fade into a light theme.
static QColor readableAccent(QColor accent, const QColor &window)
{
    const bool lightWindow = window.lightnessF() >= 0.5;
    float h, s, l, a;
    accent.getHslF(&h, &s, &l, &a);
    for (int i = 0; i < 50 && contrast(accent, window) < 3.0; ++i) {
        l = std::clamp(l + (lightWindow ? -0.02f : 0.02f), 0.0f, 1.0f);
        accent = QColor::fromHslF(h, s, l);
    }
    return accent;
}

static QVector4D straight(const QColor &c, qreal alpha)
{
    return QVector4D(c.redF(), c.greenF(), c.blueF(), alpha);
}

OutlineEffect::OutlineEffect()
{
    reconfigure(ReconfigureAll);

    m_globalsWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    connect(m_globalsWatcher.data(), &KConfigWatcher::configChanged, this, [this] {
        readColours();
        effects->addRepaintFull();
    });

    connect(effects, &EffectsHandler::windowActivated, this, [this](EffectWindow *w) {
        setFocus(WildOS::focusAfterActivation(w, m_focus));
    });
    connect(effects, &EffectsHandler::windowAdded, this, &OutlineEffect::track);
    connect(effects, &EffectsHandler::windowClosed, this, [this](EffectWindow *w) {
        repaintRing(w);
        if (m_focus == w) {
            m_focus = nullptr;
        }
    });
    connect(effects, &EffectsHandler::windowDeleted, this, [this](EffectWindow *w) {
        m_progress.remove(w);
        if (m_focus == w) {
            m_focus = nullptr;
        }
    });
    connect(effects, &EffectsHandler::screenLockingChanged, this, [] {
        effects->addRepaintFull();
    });

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        track(w);
    }
    m_focus = WildOS::focusAfterActivation(effects->activeWindow(), nullptr);
    if (m_focus) {
        m_progress.insert(m_focus, 1.0);
    }
    effects->addRepaintFull();
}

OutlineEffect::~OutlineEffect()
{
    if (m_shader) {
        effects->makeOpenGLContextCurrent();
        m_shader.reset();
    }
    effects->addRepaintFull();
}

void OutlineEffect::track(EffectWindow *w)
{
    connect(w, &EffectWindow::windowFrameGeometryChanged, this, [this](EffectWindow *w, const QRectF &old) {
        // A window without a shadow has nothing of its own painted where
        // its outline was, so the old place is cleared by hand.
        effects->addRepaint(ringRect(w, old).toAlignedRect());
        repaintRing(w);
    });
    connect(w, &EffectWindow::windowFullScreenChanged, this, &OutlineEffect::repaintRing);
    connect(w, &EffectWindow::windowMaximizedStateChanged, this, [this](EffectWindow *w) {
        repaintRing(w);
    });
    connect(w, &EffectWindow::minimizedChanged, this, [this](EffectWindow *w) {
        // KWin only says which window is next if there is one.
        if (w == m_focus && w->isMinimized()) {
            setFocus(nullptr);
        }
        repaintRing(w);
    });
    repaintRing(w);
}

bool OutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OutlineEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    m_duration = animationTime(std::chrono::milliseconds(150));
    readColours();
    effects->addRepaintFull();
}

void OutlineEffect::readColours()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->reparseConfiguration();
    QColor window = readRgb(config->group(QStringLiteral("Colors:Window")), "BackgroundNormal");
    if (!window.isValid()) {
        window = QColor(21, 26, 47);
    }
    QColor accent = readRgb(config->group(QStringLiteral("General")), "AccentColor");
    if (!accent.isValid()) {
        accent = readRgb(config->group(QStringLiteral("Colors:Selection")), "BackgroundNormal");
    }
    if (!accent.isValid()) {
        accent = QColor(0x8f, 0xb4, 0xff);
    }
    m_accent = straight(readableAccent(accent, window), 1.0);
    if (window.lightnessF() >= 0.5) {
        m_line = straight(QColor(21, 27, 44), 0.75);
        m_edge = straight(Qt::white, 0.7);
    } else {
        m_line = straight(Qt::white, 0.75);
        m_edge = straight(Qt::black, 0.6);
    }
}

bool OutlineEffect::outlined(EffectWindow *w) const
{
    if (!WildOS::isWorkWindow(w) || w->isDeleted() || w->isMinimized() || w->isFullScreen()) {
        return false;
    }
    // Minimise and restore animations bend the window into the taskbar;
    // an outline that kept its shape would hang in the air.
    if (w->data(WindowMinimizedGrabRole).value<void *>() || w->data(WindowUnminimizedGrabRole).value<void *>()) {
        return false;
    }
    return true;
}

OutlineEffect::Shape OutlineEffect::shapeFor(EffectWindow *w) const
{
    const Window *window = w->window();
    if (window && (window->maximizeMode() == MaximizeFull || window->tile() || window->quickTileMode() != QuickTileMode(QuickTileFlag::None))) {
        return Shape{.inset = true, .radius = 0};
    }
    return Shape{.inset = false, .radius = w->hasDecoration() ? s_decoratedRadius : s_clientRadius};
}

QRectF OutlineEffect::ringRect(EffectWindow *w, const QRectF &frame) const
{
    if (shapeFor(w).inset) {
        return frame;
    }
    const qreal reach = s_focusWidth + s_edgeWidth + 1.0;
    return frame.adjusted(-reach, -reach, reach, reach);
}

void OutlineEffect::repaintRing(EffectWindow *w)
{
    if (w) {
        effects->addRepaint(ringRect(w, w->frameGeometry()).toAlignedRect());
    }
}

void OutlineEffect::setFocus(EffectWindow *w)
{
    if (w == m_focus) {
        return;
    }
    repaintRing(m_focus);
    m_focus = w;
    if (w && !m_progress.contains(w)) {
        m_progress.insert(w, 0.0);
    }
    repaintRing(w);
    m_animating = true;
}

void OutlineEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_animating) {
        const qreal elapsed = m_lastPresent.count() ? qreal((presentTime - m_lastPresent).count()) : 0.0;
        const qreal step = m_duration > 0 ? elapsed / m_duration : 1.0;
        bool moving = false;
        for (auto it = m_progress.begin(); it != m_progress.end(); ++it) {
            const qreal target = it.key() == m_focus ? 1.0 : 0.0;
            if (it.value() < target) {
                it.value() = std::min(target, it.value() + step);
            } else if (it.value() > target) {
                it.value() = std::max(target, it.value() - step);
            }
            moving = moving || it.value() != target;
        }
        m_animating = moving;
        m_lastPresent = presentTime;
    }
    effects->prePaintScreen(data, presentTime);
}

void OutlineEffect::postPaintScreen()
{
    for (auto it = m_progress.begin(); it != m_progress.end();) {
        const qreal target = it.key() == m_focus ? 1.0 : 0.0;
        if (it.value() != target) {
            repaintRing(it.key());
        }
        if (it.value() <= 0.0 && target <= 0.0) {
            it = m_progress.erase(it);
        } else {
            ++it;
        }
    }
    if (!m_animating) {
        m_lastPresent = std::chrono::milliseconds::zero();
    }
    effects->postPaintScreen();
}

void OutlineEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // The ring's own repaints are asked for when it changes (focus, moves,
    // the animation), so nothing is added to the window's damage here.
    if (outlined(w)) {
        const QRect ring = ringRect(w, w->frameGeometry()).toAlignedRect();
        // KWin clips an untransformed window to its own extent. A window
        // with a shadow covers the outline; one without needs the clip
        // lifted, at the cost of not hiding the windows under it.
        if (!w->expandedGeometry().toAlignedRect().contains(ring)) {
            data.mask |= PAINT_WINDOW_TRANSFORMED;
        }
    }
    effects->prePaintWindow(w, data, presentTime);
}

bool OutlineEffect::ensureShader()
{
    if (m_shader) {
        return true;
    }
    if (m_shaderFailed) {
        return false;
    }
    ensureResources();
    QFile file(QStringLiteral(":/wildos/outline/outline.frag"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(WILDOS_OUTLINE) << "Could not read the outline shader";
        m_shaderFailed = true;
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
        header += QByteArrayLiteral("#define IN in\n#define FRAG_COLOR fragColor\nout vec4 fragColor;\n");
    } else {
        header += QByteArrayLiteral("#define IN varying\n#define FRAG_COLOR gl_FragColor\n");
    }
    // MapTexture only for the generated vertex shader's texcoord, which
    // carries each point's place relative to the frame.
    m_shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace, QByteArray(), header + file.readAll());
    if (!m_shader || !m_shader->isValid()) {
        qCWarning(WILDOS_OUTLINE) << "The outline shader failed to build";
        m_shader.reset();
        m_shaderFailed = true;
        return false;
    }
    m_frameSizeLocation = m_shader->uniformLocation("frameSize");
    m_radiusLocation = m_shader->uniformLocation("radius");
    m_insetLocation = m_shader->uniformLocation("inset");
    m_mainWidthLocation = m_shader->uniformLocation("mainWidth");
    m_edgeWidthLocation = m_shader->uniformLocation("edgeWidth");
    m_mainColourLocation = m_shader->uniformLocation("mainColour");
    m_edgeColourLocation = m_shader->uniformLocation("edgeColour");
    m_devicePixelLocation = m_shader->uniformLocation("devicePixel");
    return true;
}

static void appendRect(GLVertex2D *map, int &index, const QRectF &r, qreal scale)
{
    if (r.width() <= 0 || r.height() <= 0) {
        return;
    }
    const QVector2D tl(r.left(), r.top());
    const QVector2D tr(r.right(), r.top());
    const QVector2D br(r.right(), r.bottom());
    const QVector2D bl(r.left(), r.bottom());
    const float s = scale;
    for (const QVector2D &p : {tl, br, bl, tl, tr, br}) {
        map[index++] = GLVertex2D{.position = p * s, .texcoord = p};
    }
}

void OutlineEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(renderTarget, viewport, w, mask, region, data);

    if (!outlined(w) || data.opacity() <= 0 || !ensureShader()) {
        return;
    }

    const qreal scale = viewport.scale();
    const QRectF frame = snapToPixels(w->frameGeometry(), scale);
    const Shape shape = shapeFor(w);
    const qreal focus = m_progress.value(w, 0.0);
    const qreal mainWidth = s_lineWidth + (s_focusWidth - s_lineWidth) * focus;
    const qreal reach = mainWidth + s_edgeWidth + 1.0;

    // Four strips round the frame, so the shader runs only where the ring
    // can be and never across the window itself. Outside the frame they
    // reach in as far as the corner radius, where the ring curves in.
    const QRectF local(0, 0, frame.width(), frame.height());
    const QRectF outer = shape.inset ? local : local.adjusted(-reach, -reach, reach, reach);
    const qreal in = shape.inset ? reach : shape.radius;
    const QRectF inner = local.adjusted(in, in, -in, -in);

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
    const auto map = vbo->map<GLVertex2D>(24);
    if (!map) {
        return;
    }
    int count = 0;
    appendRect(map->data(), count, QRectF(outer.left(), outer.top(), outer.width(), inner.top() - outer.top()), scale);
    appendRect(map->data(), count, QRectF(outer.left(), inner.bottom(), outer.width(), outer.bottom() - inner.bottom()), scale);
    appendRect(map->data(), count, QRectF(outer.left(), inner.top(), inner.left() - outer.left(), inner.height()), scale);
    appendRect(map->data(), count, QRectF(inner.right(), inner.top(), outer.right() - inner.right(), inner.height()), scale);
    vbo->unmap();
    vbo->bindArrays();

    const QVector4D main = m_line + (m_accent - m_line) * focus;

    ShaderBinder binder(m_shader.get());
    QMatrix4x4 mvp = viewport.projectionMatrix();
    mvp.translate(std::round(frame.x() * scale), std::round(frame.y() * scale));
    m_shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp * data.toMatrix(scale));
    m_shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    m_shader->setUniform(m_frameSizeLocation, QVector2D(frame.width(), frame.height()));
    m_shader->setUniform(m_radiusLocation, float(shape.radius));
    m_shader->setUniform(m_insetLocation, shape.inset ? 1.0f : 0.0f);
    m_shader->setUniform(m_mainWidthLocation, float(mainWidth));
    m_shader->setUniform(m_edgeWidthLocation, float(s_edgeWidth));
    m_shader->setUniform(m_mainColourLocation, QVector4D(main.x(), main.y(), main.z(), main.w() * data.opacity()));
    m_shader->setUniform(m_edgeColourLocation, QVector4D(m_edge.x(), m_edge.y(), m_edge.z(), m_edge.w() * data.opacity()));
    m_shader->setUniform(m_devicePixelLocation, float(1.0 / scale));

    const bool clipping = region != infiniteRegion();
    const QRegion clipRegion = clipping ? viewport.mapToRenderTarget(region) : infiniteRegion();
    if (clipping) {
        glEnable(GL_SCISSOR_TEST);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    vbo->draw(clipRegion, GL_TRIANGLES, 0, count, clipping);
    glDisable(GL_BLEND);
    if (clipping) {
        glDisable(GL_SCISSOR_TEST);
    }
    vbo->unbindArrays();
}

bool OutlineEffect::isActive() const
{
    return !effects->isScreenLocked();
}

int OutlineEffect::requestedEffectChainPosition() const
{
    // Before the background blur (58), so the outline is drawn on top of a
    // blurred window rather than blurred with it, and before the corner
    // effect (60).
    return 55;
}

KWIN_EFFECT_FACTORY_SUPPORTED(OutlineEffect, "metadata.json", return OutlineEffect::supported();)

} // namespace KWin

#include "outline.moc"
