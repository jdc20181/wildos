/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gestures.h"

// KWin 6.3 has no setting for libinput's three-finger drag, and does not
// install its libinput device header. This is that header from the same
// KWin release, for the one accessor that hands over the libinput device;
// the package depends on exactly the libkwin6 it was built against.
#include "kwin-6.3.6/backends/libinput/device.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <effect/effecttogglablestate.h>
#include <pointer_input.h>
#include <virtualdesktops.h>
#include <window.h>
#include <workspace.h>

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <libinput.h>
#include <linux/input-event-codes.h>

#include <algorithm>
#include <utility>
#include <cmath>

using namespace std::chrono_literals;

namespace WildOS
{

using KWin::EffectWindow;
using KWin::Window;

namespace
{
const QString configFile = QStringLiteral("wildosrc");
const QString configGroup = QStringLiteral("Gestures");

// How far the fingers travel for a swipe to go all the way, in libinput's
// gesture units, which it scales from each touchpad's own size to about 9.5
// a millimetre: some 26 mm, a third of a laptop touchpad, as on a Mac. The
// 30% that commits a swipe is then about 8 mm.
constexpr qreal swipeDistance = 250.0;
// Every swipe completes past 30% of the way, or on a quick flick.
constexpr qreal commitProgress = 0.3;
// A flick is the last 60 ms moving at two full gestures a second or more.
// Moving back that fast undoes the gesture wherever the fingers are.
constexpr qreal flickSpeed = 2.0;
constexpr auto flickWindow = 60ms;
// Fingers must move this far before a swipe's direction is read, so a
// slightly crooked start does not pick the wrong one.
constexpr qreal decideDistance = 12.0;

GesturesEffect::Action parseAction(const QString &value, GesturesEffect::Action fallback)
{
    using Action = GesturesEffect::Action;
    static const QHash<QString, Action> names = {
        {QStringLiteral("nothing"), Action::Nothing},
        {QStringLiteral("taskview"), Action::TaskView},
        {QStringLiteral("desktop"), Action::ShowDesktop},
        {QStringLiteral("desktops"), Action::SwitchDesktops},
        {QStringLiteral("appgrid"), Action::AppGrid},
        {QStringLiteral("notifications"), Action::Notifications},
        {QStringLiteral("history"), Action::BackForward},
    };
    return names.value(value, fallback);
}

qreal easeOutCubic(qreal t)
{
    const qreal u = 1.0 - t;
    return 1.0 - u * u * u;
}

qreal lerp(qreal a, qreal b, qreal t)
{
    return a + (b - a) * t;
}
}

GesturesEffect::GesturesEffect()
    : KWin::InputEventFilter(KWin::InputFilterOrder::GlobalShortcut)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig(configFile)))
    , m_showDesktopAction(std::make_unique<QAction>())
{
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == configGroup) {
            loadConfig();
        }
    });
    loadConfig();

    // The same name the Show desktop KWin script used, so a key someone set
    // for it in Settings keeps working.
    m_showDesktopAction->setObjectName(QStringLiteral("Wild OS show desktop"));
    m_showDesktopAction->setText(QStringLiteral("Show the desktop (minimise every window)"));
    KGlobalAccel::self()->setDefaultShortcut(m_showDesktopAction.get(), {Qt::META | Qt::Key_D});
    KGlobalAccel::self()->setShortcut(m_showDesktopAction.get(), {Qt::META | Qt::Key_D});
    connect(m_showDesktopAction.get(), &QAction::triggered, this, &GesturesEffect::toggleShowDesktop);

    auto ws = KWin::workspace();
    for (Window *window : ws->windows()) {
        watch(window);
    }
    connect(ws, &KWin::Workspace::windowAdded, this, [this](Window *window) {
        watch(window);
        if (window->isNormalWindow() && !window->skipTaskbar()) {
            forget();
        }
    });
    connect(ws, &KWin::Workspace::currentDesktopChanged, this, &GesturesEffect::forget);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, [this](EffectWindow *w) {
        m_transformed.remove(w);
    });

    connect(KWin::input(), &KWin::InputRedirection::deviceAdded, this, [this](KWin::InputDevice *device) {
        applyThreeFingerDrag(device);
    });

    KWin::input()->installInputEventFilter(this);
}

GesturesEffect::~GesturesEffect()
{
    KWin::input()->uninstallInputEventFilter(this);
    finishAnimation();
    clearTransform();
}

void GesturesEffect::loadConfig()
{
    const KConfigGroup group = m_configWatcher->config()->group(configGroup);
    // Three and four fingers each have their own swipes. Settings before
    // that had one set for whichever count Fingers chose, which carries
    // over to that count, the other count doing nothing.
    const bool oldFour = group.readEntry("Fingers", 3) == 4;
    const auto read = [&group, oldFour](const char *direction, int fingers, Action fallback) {
        const QString old = group.readEntry(QStringLiteral("Swipe%1").arg(QLatin1String(direction)).toUtf8().constData(), QString());
        const bool chosenBefore = (fingers == 4) == oldFour;
        const Action carried = chosenBefore ? parseAction(old, fallback) : (fingers == 3 ? Action::Nothing : fallback);
        const QString key = QStringLiteral("%1%2").arg(fingers == 3 ? QStringLiteral("Three") : QStringLiteral("Four"), QLatin1String(direction));
        return parseAction(group.readEntry(key.toUtf8().constData(), QString()), carried);
    };
    for (int fingers : {3, 4}) {
        m_actions[fingers] = {
            {Gesture::SwipeUp, read("Up", fingers, Action::TaskView)},
            {Gesture::SwipeDown, read("Down", fingers, Action::ShowDesktop)},
            {Gesture::SwipeSideways, read("Sideways", fingers, Action::SwitchDesktops)},
        };
    }
    const bool drag = group.readEntry("ThreeFingerDrag", false);
    if (drag != m_threeFingerDrag) {
        m_threeFingerDrag = drag;
        applyThreeFingerDrag();
    }
}

void GesturesEffect::applyThreeFingerDrag()
{
    for (KWin::InputDevice *device : KWin::input()->devices()) {
        applyThreeFingerDrag(device);
    }
}

// Set on the libinput device itself, which keeps it until the touchpad is
// unplugged; a touchpad that comes back gets it again from deviceAdded.
void GesturesEffect::applyThreeFingerDrag(KWin::InputDevice *device)
{
    auto libinputDevice = qobject_cast<KWin::LibInput::Device *>(device);
    if (!libinputDevice || !libinputDevice->isTouchpad()) {
        return;
    }
    libinput_device *handle = libinputDevice->device();
    if (!handle || libinput_device_config_3fg_drag_get_finger_count(handle) < 3) {
        return;
    }
    libinput_device_config_3fg_drag_set_enabled(handle, m_threeFingerDrag ? LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG : LIBINPUT_CONFIG_3FG_DRAG_DISABLED);
}

GesturesEffect::Action GesturesEffect::actionFor(Gesture gesture) const
{
    return m_actions.value(m_fingerCount).value(gesture, Action::Nothing);
}

GesturesEffect::Gesture GesturesEffect::opposite(Gesture gesture) const
{
    switch (gesture) {
    case Gesture::SwipeUp:
        return Gesture::SwipeDown;
    case Gesture::SwipeDown:
        return Gesture::SwipeUp;
    default:
        return Gesture::None;
    }
}

// Whatever a gesture opened, the opposite gesture closes, as on a Mac:
// swiping down closes Task view opened by swiping up, and swiping up brings
// back the windows that swiping down put away.
bool GesturesEffect::assignedOpposite(Gesture gesture, Action action) const
{
    const Gesture other = opposite(gesture);
    return other != Gesture::None && actionFor(other) == action;
}

KWin::EffectTogglableState *GesturesEffect::overviewState() const
{
    KWin::Effect *overview = KWin::effects->findEffect(QStringLiteral("overview"));
    if (!overview) {
        return nullptr;
    }
    // Overview's first state is the one between no overview and overview;
    // the others go on to its grid of desktops.
    const auto states = overview->findChildren<KWin::EffectTogglableState *>(QString(), Qt::FindDirectChildrenOnly);
    return states.value(0);
}

bool GesturesEffect::taskViewOpen() const
{
    KWin::EffectTogglableState *state = overviewState();
    return state && state->status() == KWin::EffectTogglableState::Status::Active;
}

bool GesturesEffect::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    if (fingerCount < 3) {
        return false;
    }
    begin(fingerCount);
    return true;
}

bool GesturesEffect::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
    if (!m_tracking) {
        return false;
    }
    m_delta += delta;
    if (m_gesture == Gesture::None) {
        if (!m_counted || std::abs(m_delta.x()) + std::abs(m_delta.y()) < decideDistance) {
            return true;
        }
        if (std::abs(m_delta.x()) > std::abs(m_delta.y())) {
            decide(Gesture::SwipeSideways);
        } else {
            decide(m_delta.y() < 0 ? Gesture::SwipeUp : Gesture::SwipeDown);
        }
    }
    switch (m_gesture) {
    case Gesture::SwipeUp:
        update(-m_delta.y(), time);
        break;
    case Gesture::SwipeDown:
        update(m_delta.y(), time);
        break;
    default:
        // Positive when the fingers go left, which brings in the desktop on
        // the right, as on a Mac.
        update(-m_delta.x(), time);
        break;
    }
    return true;
}

bool GesturesEffect::swipeGestureEnd(std::chrono::microseconds time)
{
    if (!m_tracking) {
        return false;
    }
    end(false, time);
    return true;
}

bool GesturesEffect::swipeGestureCancelled(std::chrono::microseconds time)
{
    if (!m_tracking) {
        return false;
    }
    end(true, time);
    return true;
}

// A thumb and three fingers pinching never reaches here reliably: libinput
// takes a thumb that stays still for a resting one, drops it and reports
// the three fingers as a swipe; only when the thumb moves too does a
// four-finger pinch arrive. A gesture that works some of the time is worse
// than none, so pinches of three or more fingers are taken and do nothing,
// as KWin's own would have.
bool GesturesEffect::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    if (fingerCount < 3) {
        return false;
    }
    m_pinching = true;
    return true;
}

bool GesturesEffect::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
    return m_pinching;
}

bool GesturesEffect::pinchGestureEnd(std::chrono::microseconds time)
{
    return std::exchange(m_pinching, false);
}

bool GesturesEffect::pinchGestureCancelled(std::chrono::microseconds time)
{
    return std::exchange(m_pinching, false);
}

// Every swipe of three or more fingers stops here, so KWin's own
// four-finger gestures never run next to these. Three and four fingers each
// have their own actions; five count as four.
void GesturesEffect::begin(int fingerCount)
{
    finishAnimation();
    m_tracking = true;
    // Three-finger drag takes every three-finger movement, so only
    // four-finger swipes are left while it is on.
    m_fingerCount = std::min(fingerCount, 4);
    m_counted = fingerCount >= 4 || (fingerCount == 3 && !m_threeFingerDrag);
    m_gesture = Gesture::None;
    m_mode = Mode::Ignore;
    m_action = Action::Nothing;
    m_delta = QPointF();
    m_amount = 0;
    m_samples.clear();
}

void GesturesEffect::decide(Gesture gesture)
{
    m_gesture = gesture;
    m_mode = Mode::Ignore;
    if (KWin::effects->isScreenLocked()) {
        return;
    }

    if (taskViewOpen()) {
        if (assignedOpposite(gesture, Action::TaskView)) {
            m_mode = Mode::CloseTaskView;
        }
        return;
    }
    if (desktopShown() && assignedOpposite(gesture, Action::ShowDesktop)) {
        QList<Window *> windows;
        for (const QPointer<Window> &window : std::as_const(m_saved)) {
            if (window && window->isMinimized()) {
                windows << window;
            }
        }
        if (!windows.isEmpty()) {
            m_mode = Mode::ShowWindows;
            startDesktopTransform(windows, true);
            setDesktopProgress(1.0);
        }
        return;
    }

    m_action = actionFor(gesture);
    switch (m_action) {
    case Action::TaskView:
        if (overviewState()) {
            m_mode = Mode::OpenTaskView;
        }
        break;
    case Action::ShowDesktop:
        if (!desktopShown()) {
            const QList<Window *> windows = minimisable();
            if (!windows.isEmpty()) {
                m_mode = Mode::HideWindows;
                startDesktopTransform(windows, false);
            }
        }
        break;
    case Action::SwitchDesktops:
        if (gesture == Gesture::SwipeSideways) {
            m_mode = Mode::SlideDesktops;
        }
        break;
    case Action::BackForward:
        if (gesture == Gesture::SwipeSideways) {
            m_mode = Mode::AtEnd;
        }
        break;
    case Action::AppGrid:
    case Action::Notifications:
        m_mode = Mode::AtEnd;
        break;
    case Action::Nothing:
        break;
    }
}

qreal GesturesEffect::progressOf(qreal amount) const
{
    if (m_gesture == Gesture::SwipeSideways) {
        return std::clamp(amount / swipeDistance, -1.0, 1.0);
    }
    return std::clamp(amount / swipeDistance, 0.0, 1.0);
}

void GesturesEffect::update(qreal amount, std::chrono::microseconds time)
{
    m_amount = amount;
    m_samples.append({time, amount});
    while (m_samples.size() > 2 && time - m_samples.first().time > 4 * flickWindow) {
        m_samples.removeFirst();
    }

    const qreal progress = progressOf(amount);
    KWin::EffectTogglableState *state = nullptr;
    switch (m_mode) {
    case Mode::OpenTaskView:
        if ((state = overviewState())) {
            state->setStatus(KWin::EffectTogglableState::Status::Activating);
            state->setInProgress(true);
            state->setPartialActivationFactor(progress);
        }
        break;
    case Mode::CloseTaskView:
        if ((state = overviewState())) {
            state->setStatus(KWin::EffectTogglableState::Status::Deactivating);
            state->setInProgress(true);
            state->setPartialActivationFactor(1.0 - progress);
        }
        break;
    case Mode::HideWindows:
        setDesktopProgress(progress);
        break;
    case Mode::ShowWindows:
        setDesktopProgress(1.0 - progress);
        break;
    case Mode::SlideDesktops:
        slideDesktops(progress);
        break;
    case Mode::Ignore:
    case Mode::AtEnd:
        break;
    }
}

// Gestures a second along the gesture, from the samples of the last 60 ms.
qreal GesturesEffect::velocity(std::chrono::microseconds now) const
{
    if (m_samples.size() < 2) {
        return 0;
    }
    const Sample &last = m_samples.last();
    const Sample *first = &m_samples.first();
    for (const Sample &sample : m_samples) {
        if (now - sample.time <= flickWindow) {
            first = &sample;
            break;
        }
    }
    const auto elapsed = std::chrono::duration<qreal>(last.time - first->time).count();
    if (elapsed <= 0) {
        return 0;
    }
    return (progressOf(last.amount) - progressOf(first->amount)) / elapsed;
}

void GesturesEffect::end(bool cancelled, std::chrono::microseconds time)
{
    m_tracking = false;
    const qreal progress = progressOf(m_amount);
    const qreal sign = progress < 0 ? -1.0 : 1.0;
    const qreal along = std::abs(progress);
    const qreal speed = velocity(time) * sign;
    const bool commit = !cancelled && along > 0.0 && (speed >= flickSpeed || (speed > -flickSpeed && along >= commitProgress));

    KWin::EffectTogglableState *state = overviewState();
    switch (m_mode) {
    case Mode::OpenTaskView:
        if (state) {
            commit ? state->activate() : state->deactivate();
        }
        break;
    case Mode::CloseTaskView:
        if (state) {
            commit ? state->deactivate() : state->activate();
        }
        break;
    case Mode::HideWindows:
        if (commit) {
            animateDesktop(1.0, [this] {
                hideWindows(minimisable(), false);
            });
        } else {
            animateDesktop(0.0, [this] {
                clearTransform();
            });
        }
        break;
    case Mode::ShowWindows:
        if (commit) {
            animateDesktop(0.0, [this] {
                showWindows(false);
            });
        } else {
            animateDesktop(1.0, [this] {
                clearTransform();
            });
        }
        break;
    case Mode::SlideDesktops:
        endSlide(commit, progress);
        break;
    case Mode::AtEnd:
        if (!commit) {
            break;
        }
        if (m_action == Action::AppGrid) {
            callShell(QStringLiteral("ToggleAllApps"));
        } else if (m_action == Action::Notifications) {
            callShell(QStringLiteral("ToggleNotifications"));
        } else if (m_action == Action::BackForward) {
            // Fingers going right go back, as a page turned on a Mac does.
            clickHistory(progress < 0, time);
        }
        break;
    case Mode::Ignore:
        break;
    }
    m_mode = Mode::Ignore;
    m_samples.clear();
}

// Show desktop, as Windows does it: every window goes into the taskbar, and
// doing it again brings back exactly those windows, in the order they were
// stacked, with the one that was in use in front. Opening or restoring a
// window in between starts afresh.
QList<Window *> GesturesEffect::minimisable() const
{
    QList<Window *> windows;
    for (Window *window : KWin::workspace()->stackingOrder()) {
        if (window->isClient() && !window->isDeleted() && window->isNormalWindow() && window->isMinimizable() && !window->isMinimized()
            && !window->skipTaskbar() && !window->isTransient() && window->isOnCurrentDesktop()) {
            windows << window;
        }
    }
    return windows;
}

bool GesturesEffect::desktopShown() const
{
    const bool anySaved = std::any_of(m_saved.cbegin(), m_saved.cend(), [](const QPointer<Window> &w) {
        return !w.isNull();
    });
    return anySaved && minimisable().isEmpty();
}

void GesturesEffect::toggleShowDesktop()
{
    if (m_tracking) {
        return;
    }
    finishAnimation();
    if (desktopShown()) {
        showWindows(true);
    } else {
        const QList<Window *> windows = minimisable();
        if (!windows.isEmpty()) {
            hideWindows(windows, true);
        }
    }
}

// `animated` is false after a gesture, which has already drawn the windows
// into the taskbar: being the full screen effect for that moment stops the
// minimise animation from playing it a second time.
void GesturesEffect::hideWindows(const QList<Window *> &windows, bool animated)
{
    Window *active = KWin::workspace()->activeWindow();
    m_busy = true;
    if (!animated) {
        KWin::effects->setActiveFullScreenEffect(this);
    }
    for (auto it = windows.crbegin(); it != windows.crend(); ++it) {
        (*it)->setMinimized(true);
    }
    if (!animated) {
        KWin::effects->setActiveFullScreenEffect(nullptr);
    }
    m_busy = false;
    m_saved.clear();
    for (Window *window : windows) {
        m_saved << window;
    }
    m_savedActive = windows.contains(active) ? active : (windows.isEmpty() ? nullptr : windows.last());
    clearTransform();
}

void GesturesEffect::showWindows(bool animated)
{
    QList<Window *> windows;
    for (const QPointer<Window> &window : std::as_const(m_saved)) {
        if (window && window->isMinimized()) {
            windows << window;
        }
    }
    QPointer<Window> active = m_savedActive;
    forget();

    m_busy = true;
    if (!animated) {
        KWin::effects->setActiveFullScreenEffect(this);
    }
    for (Window *window : windows) {
        window->setMinimized(false);
    }
    if (!animated) {
        KWin::effects->setActiveFullScreenEffect(nullptr);
    }
    for (Window *window : windows) {
        KWin::workspace()->raiseWindow(window);
    }
    m_busy = false;
    if (active && windows.contains(active.data())) {
        KWin::workspace()->activateWindow(active);
    }
    clearTransform();
}

void GesturesEffect::forget()
{
    m_saved.clear();
    m_savedActive.clear();
}

void GesturesEffect::watch(Window *window)
{
    connect(window, &Window::minimizedChanged, this, [this, window] {
        if (!m_busy && !window->isMinimized()) {
            forget();
        }
    });
}

// The windows a gesture moves, with their dialogs, which follow them into
// the taskbar.
void GesturesEffect::startDesktopTransform(const QList<Window *> &windows, bool minimised)
{
    clearTransform();
    QList<EffectWindow *> mains;
    for (Window *window : windows) {
        if (EffectWindow *w = window->effectWindow()) {
            mains << w;
        }
    }
    for (EffectWindow *w : KWin::effects->stackingOrder()) {
        EffectWindow *main = w;
        while (main->transientFor()) {
            main = main->transientFor();
        }
        if (!mains.contains(main) || w->isMinimized() != minimised || w->isDeleted()) {
            continue;
        }
        m_transformed.insert(w, minimised ? std::make_shared<KWin::EffectWindowVisibleRef>(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE) : nullptr);
    }
    m_desktopProgress = 0;
}

void GesturesEffect::setDesktopProgress(qreal progress)
{
    m_desktopProgress = progress;
    KWin::effects->addRepaintFull();
}

void GesturesEffect::animateDesktop(qreal to, std::function<void()> done)
{
    const qreal distance = std::abs(to - m_desktopProgress);
    const auto duration = std::chrono::milliseconds(int(std::max(80.0, 250.0 * distance) * KWin::effects->animationTimeFactor()));
    m_animation.running = true;
    m_animation.from = m_desktopProgress;
    m_animation.to = to;
    m_animation.start = std::chrono::milliseconds(-1);
    m_animation.ending = false;
    m_animation.duration = duration;
    m_animation.done = std::move(done);
    if (duration.count() <= 0) {
        finishAnimation();
        return;
    }
    KWin::effects->addRepaintFull();
}

void GesturesEffect::finishAnimation()
{
    if (!m_animation.running) {
        return;
    }
    m_animation.running = false;
    setDesktopProgress(m_animation.to);
    auto done = std::move(m_animation.done);
    m_animation.done = nullptr;
    if (done) {
        done();
    }
}

void GesturesEffect::clearTransform()
{
    m_transformed.clear();
    m_desktopProgress = 0;
    KWin::effects->addRepaintFull();
}

void GesturesEffect::slideDesktops(qreal offset)
{
    auto desktops = KWin::VirtualDesktopManager::self();
    if (desktops->grid().width() > 1) {
        Q_EMIT desktops->currentChanging(desktops->currentDesktop(), QPointF(offset, 0));
    }
}

void GesturesEffect::endSlide(bool commit, qreal offset)
{
    auto desktops = KWin::VirtualDesktopManager::self();
    KWin::VirtualDesktop *current = desktops->currentDesktop();
    KWin::VirtualDesktop *target = current;
    if (commit) {
        const bool wrap = desktops->isNavigationWrappingAround();
        target = offset > 0 ? desktops->toRight(current, wrap) : desktops->toLeft(current, wrap);
    }
    if (target != current) {
        desktops->setCurrent(target);
    } else {
        Q_EMIT desktops->currentChangingCancelled();
    }
}

// Start and the clock's flyout live in plasmashell, which opens them on the
// screen holding the point given (org.wildos.shell's Shell object, shared
// with the hot corners). Asynchronous, so a busy plasmashell never holds up
// the compositor.
void GesturesEffect::callShell(const QString &method)
{
    const QPointF cursor = KWin::effects->cursorPos();
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/org/wildos/Shell"),
                                                       QStringLiteral("org.wildos.Shell"), method);
    call << int(cursor.x()) << int(cursor.y());
    QDBusConnection::sessionBus().asyncCall(call);
}

// The mouse's back and forward buttons, which Files, Settings, browsers and
// Windows mode apps already understand, sent to the window under the
// pointer.
void GesturesEffect::clickHistory(bool back, std::chrono::microseconds time)
{
    const uint32_t button = back ? BTN_SIDE : BTN_EXTRA;
    auto pointer = KWin::input()->pointer();
    pointer->processButton(button, KWin::PointerButtonState::Pressed, time);
    pointer->processFrame();
    pointer->processButton(button, KWin::PointerButtonState::Released, time);
    pointer->processFrame();
}

void GesturesEffect::prePaintScreen(KWin::ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_animation.running) {
        if (m_animation.start.count() < 0) {
            m_animation.start = presentTime;
        }
        const qreal t = std::clamp(qreal((presentTime - m_animation.start).count()) / qreal(m_animation.duration.count()), 0.0, 1.0);
        m_desktopProgress = lerp(m_animation.from, m_animation.to, easeOutCubic(t));
        // The last frame is drawn with the windows still at the taskbar;
        // they are minimised after it (postPaintScreen), or the frame in
        // between would show them back at full size.
        m_animation.ending = t >= 1.0;
    }
    if (!m_transformed.isEmpty()) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    KWin::effects->prePaintScreen(data, presentTime);
}

void GesturesEffect::postPaintScreen()
{
    if (m_animation.running && m_animation.ending) {
        finishAnimation();
    } else if (m_animation.running) {
        KWin::effects->addRepaintFull();
    }
    KWin::effects->postPaintScreen();
}

void GesturesEffect::prePaintWindow(EffectWindow *w, KWin::WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_transformed.contains(w)) {
        data.setTransformed();
    }
    KWin::effects->prePaintWindow(w, data, presentTime);
}

// Each window shrinks towards its taskbar button and fades as it goes,
// keeping its shape, the way Windows minimises.
void GesturesEffect::paintWindow(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport, EffectWindow *w, int mask, QRegion region,
                                 KWin::WindowPaintData &data)
{
    if (m_desktopProgress > 0 && m_transformed.contains(w)) {
        const QRectF frame = w->frameGeometry();
        EffectWindow *main = w;
        while (main->transientFor()) {
            main = main->transientFor();
        }
        QRectF icon = main->iconGeometry();
        if (icon.isEmpty()) {
            const QRectF screen = w->screen() ? w->screen()->geometry() : frame;
            icon = QRectF(screen.center().x() - 24, screen.bottom() - 40, 48, 32);
        }
        if (!frame.isEmpty()) {
            const qreal p = m_desktopProgress;
            const qreal end = std::min(icon.width() / frame.width(), icon.height() / frame.height());
            const qreal scale = lerp(1.0, end, p);
            const QPointF centre(lerp(frame.center().x(), icon.center().x(), p), lerp(frame.center().y(), icon.center().y(), p));
            data.setXScale(data.xScale() * scale);
            data.setYScale(data.yScale() * scale);
            data.translate(centre.x() - frame.width() * scale / 2 - frame.x(), centre.y() - frame.height() * scale / 2 - frame.y());
            data.multiplyOpacity(1.0 - 0.8 * p);
        }
    }
    KWin::effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

bool GesturesEffect::isActive() const
{
    return !m_transformed.isEmpty() || m_animation.running;
}

// Ahead of the minimise animation and the effects that draw windows
// elsewhere, so they see the window where it now is.
int GesturesEffect::requestedEffectChainPosition() const
{
    return 50;
}

}

KWIN_EFFECT_FACTORY(WildOS::GesturesEffect, "metadata.json")

#include "gestures.moc"
