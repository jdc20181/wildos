/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <effect/effect.h>

#include <KConfigWatcher>

#include <QColor>
#include <QHash>
#include <QVector4D>

#include <chrono>
#include <memory>

namespace KWin
{

class GLShader;
class EffectWindow;

// Settings > Accessibility > Outline windows, for people who cannot see
// where one window ends and the next begins, or which one has focus: a
// two-tone edge round every window, light against dark and dark against
// light so it shows on any wallpaper or window, and a thick ring in the
// accent colour round the window in use.
//
// It is drawn by KWin round each window's frame rather than by the title
// bar theme, so windows that draw their own title bar (Wild OS apps, GTK
// apps, browsers, Windows mode) get the same outline as the rest.
class OutlineEffect : public Effect
{
    Q_OBJECT

public:
    OutlineEffect();
    ~OutlineEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

private:
    struct Shape {
        // Inset rings sit just inside the frame of a maximised or snapped
        // window, which has no room round it.
        bool inset = false;
        qreal radius = 0;
    };

    void track(EffectWindow *w);
    bool outlined(EffectWindow *w) const;
    Shape shapeFor(EffectWindow *w) const;
    QRectF ringRect(EffectWindow *w, const QRectF &frame) const;
    void repaintRing(EffectWindow *w);
    void setFocus(EffectWindow *w);
    void readColours();
    bool ensureShader();

    std::unique_ptr<GLShader> m_shader;
    bool m_shaderFailed = false;
    KConfigWatcher::Ptr m_globalsWatcher;

    EffectWindow *m_focus = nullptr;
    // How far each window is through becoming the one in use, 0 to 1.
    QHash<EffectWindow *, qreal> m_progress;
    std::chrono::milliseconds m_lastPresent = std::chrono::milliseconds::zero();
    bool m_animating = false;
    qreal m_duration = 150;

    QVector4D m_accent;
    QVector4D m_line;
    QVector4D m_edge;

    int m_frameSizeLocation = -1;
    int m_radiusLocation = -1;
    int m_insetLocation = -1;
    int m_mainWidthLocation = -1;
    int m_edgeWidthLocation = -1;
    int m_mainColourLocation = -1;
    int m_edgeColourLocation = -1;
    int m_devicePixelLocation = -1;
};

} // namespace KWin
