/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <effect/offscreeneffect.h>

#include <QHash>
#include <QMetaObject>

#include <memory>

namespace KWin
{

class GLShader;

// Windows 11 rounds all four corners of a window. The Wild OS Aurorae theme
// rounds the top two, where it draws the title bar, but the client below it
// is a plain rectangle. This effect cuts the bottom corners to the same
// radius, and squares the top ones while a window is tiled so it sits flush
// against its neighbours as a snapped window does on Windows.
class RoundCornersEffect : public OffscreenEffect
{
    Q_OBJECT

public:
    RoundCornersEffect();
    ~RoundCornersEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

private:
    enum class Shape {
        Untouched,
        Rounded,
        Tiled,
    };

    void track(EffectWindow *w);
    void forget(EffectWindow *w);
    void update(EffectWindow *w);
    Shape shapeFor(EffectWindow *w) const;
    bool ensureShader();
    bool readDecorationSetting();

    std::unique_ptr<GLShader> m_shader;
    bool m_shaderFailed = false;
    bool m_ourDecoration = false;
    QHash<EffectWindow *, Shape> m_shapes;
    QHash<EffectWindow *, QList<QMetaObject::Connection>> m_connections;

    int m_expandedOriginLocation = -1;
    int m_expandedSizeLocation = -1;
    int m_frameSizeLocation = -1;
    int m_radiusLocation = -1;
    int m_edgeWidthLocation = -1;
    int m_devicePixelLocation = -1;
    int m_roundBottomLocation = -1;
    int m_squareTopLocation = -1;
    int m_shadowColorLocation = -1;
    int m_shadowSigmaLocation = -1;
    int m_shadowOffsetYLocation = -1;
};

} // namespace KWin
