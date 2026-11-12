/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <effect/effect.h>
#include <effect/effectwindow.h>
#include <input.h>

#include <KConfigWatcher>

#include <QAction>
#include <QHash>
#include <QList>
#include <QPointer>

#include <chrono>
#include <functional>
#include <memory>

namespace KWin
{
class EffectTogglableState;
class InputDevice;
class Window;
}

namespace WildOS
{

// Touchpad gestures as on a Mac, read from wildosrc [Gestures], which
// Settings > Bluetooth and devices > Touchpad > Gestures writes.
//
// The effect sits in KWin's input chain just ahead of its global shortcuts,
// so it sees every swipe and pinch of three or more fingers before KWin's
// own fixed four-finger gestures do, and before any window (Windows mode's
// included) could. Two-finger scrolling, pinching and taps are not
// gestures to KWin and go to the app untouched.
//
// Swipes follow the fingers: Task view (KWin's Overview) grows, the desktops
// slide and the windows shrink towards the taskbar as the fingers move, and
// going back undoes it. On release the action happens if the fingers went
// 30% of the way or flicked forwards, and is undone otherwise.
//
// It also owns Windows key + D (Show desktop minimises every window into
// the taskbar, as on Windows), because the swipe down has to know which
// windows that put away to bring them back on the swipe up.
class GesturesEffect : public KWin::Effect, public KWin::InputEventFilter
{
    Q_OBJECT

public:
    enum class Gesture {
        None,
        SwipeUp,
        SwipeDown,
        SwipeSideways,
    };
    enum class Action {
        Nothing,
        TaskView,
        ShowDesktop,
        SwitchDesktops,
        AppGrid,
        Notifications,
        BackForward,
    };

    GesturesEffect();
    ~GesturesEffect() override;

    void prePaintScreen(KWin::ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void prePaintWindow(KWin::EffectWindow *w, KWin::WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport, KWin::EffectWindow *w, int mask, QRegion region,
                     KWin::WindowPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    bool swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    bool swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override;
    bool swipeGestureEnd(std::chrono::microseconds time) override;
    bool swipeGestureCancelled(std::chrono::microseconds time) override;
    bool pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override;
    bool pinchGestureEnd(std::chrono::microseconds time) override;
    bool pinchGestureCancelled(std::chrono::microseconds time) override;

private:
    // What a gesture in progress drives.
    enum class Mode {
        Ignore,
        OpenTaskView,
        CloseTaskView,
        HideWindows,
        ShowWindows,
        SlideDesktops,
        AtEnd,
    };

    struct Sample {
        std::chrono::microseconds time;
        qreal amount;
    };

    void loadConfig();
    void applyThreeFingerDrag();
    void applyThreeFingerDrag(KWin::InputDevice *device);
    Action actionFor(Gesture gesture) const;
    Gesture opposite(Gesture gesture) const;
    bool assignedOpposite(Gesture gesture, Action action) const;

    void begin(int fingerCount);
    void decide(Gesture gesture);
    void update(qreal amount, std::chrono::microseconds time);
    void end(bool cancelled, std::chrono::microseconds time);
    qreal progressOf(qreal amount) const;
    qreal velocity(std::chrono::microseconds now) const;

    KWin::EffectTogglableState *overviewState() const;
    bool taskViewOpen() const;

    QList<KWin::Window *> minimisable() const;
    bool desktopShown() const;
    void toggleShowDesktop();
    void hideWindows(const QList<KWin::Window *> &windows, bool animated);
    void showWindows(bool animated);
    void forget();
    void watch(KWin::Window *window);
    void startDesktopTransform(const QList<KWin::Window *> &windows, bool minimised);
    void setDesktopProgress(qreal progress);
    void animateDesktop(qreal to, std::function<void()> done);
    void finishAnimation();
    void clearTransform();

    void slideDesktops(qreal offset);
    void endSlide(bool commit, qreal offset);
    void callShell(const QString &method);
    void clickHistory(bool back, std::chrono::microseconds time);

    KConfigWatcher::Ptr m_configWatcher;
    bool m_threeFingerDrag = false;
    // Per finger count (3 or 4), what each gesture does.
    QHash<int, QHash<Gesture, Action>> m_actions;
    int m_fingerCount = 0;

    // The gesture under the fingers.
    bool m_tracking = false;
    bool m_counted = false;
    Gesture m_gesture = Gesture::None;
    Mode m_mode = Mode::Ignore;
    Action m_action = Action::Nothing;
    QPointF m_delta;
    bool m_pinching = false;
    qreal m_amount = 0;
    QList<Sample> m_samples;

    // Show desktop: the windows it minimised, bottom of the stack first,
    // and the one that was in use.
    QList<QPointer<KWin::Window>> m_saved;
    QPointer<KWin::Window> m_savedActive;
    bool m_busy = false;
    std::unique_ptr<QAction> m_showDesktopAction;

    // The windows drawn on their way to or from the taskbar, with the refs
    // that keep minimised ones drawn while they come back.
    QHash<KWin::EffectWindow *, std::shared_ptr<KWin::EffectWindowVisibleRef>> m_transformed;
    qreal m_desktopProgress = 0;

    struct Animation {
        bool running = false;
        bool ending = false;
        qreal from = 0;
        qreal to = 0;
        std::chrono::milliseconds start{-1};
        std::chrono::milliseconds duration{0};
        std::function<void()> done;
    } m_animation;
};

}
