#include "historyswipe.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QWheelEvent>

#include <cmath>

namespace
{
// How far the content would have moved, in pixels, before the swipe counts
// as a page turn; about a third of the way across a touchpad.
constexpr qreal turnDistance = 110.0;
// Movement before the swipe's direction is read.
constexpr qreal decideDistance = 8.0;
}

void HistorySwipe::install()
{
    static QPointer<HistorySwipe> instance;
    if (instance || !qGuiApp) {
        return;
    }
    // The shell and KWin draw with WildOS.UI too, but have no pages to turn.
    const QString name = QCoreApplication::applicationName();
    if (name == QLatin1String("plasmashell") || name.startsWith(QLatin1String("kwin"))) {
        return;
    }
    instance = new HistorySwipe(qGuiApp);
}

HistorySwipe::HistorySwipe(QObject *parent)
    : QObject(parent)
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc"))))
{
    connect(m_watcher.get(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Gestures")) {
            readConfig();
        }
    });
    readConfig();
    qGuiApp->installEventFilter(this);
}

void HistorySwipe::readConfig()
{
    m_enabled = m_watcher->config()->group(QStringLiteral("Gestures")).readEntry("PageSwipe", QStringLiteral("history")) == QLatin1String("history");
}

bool HistorySwipe::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel && !m_delivering && m_enabled) {
        if (auto window = qobject_cast<QQuickWindow *>(watched)) {
            return wheel(window, static_cast<QWheelEvent *>(event));
        }
    }
    return QObject::eventFilter(watched, event);
}

// Only touchpads give a scroll its phases; a mouse wheel's events have none
// and pass straight through.
bool HistorySwipe::wheel(QQuickWindow *window, QWheelEvent *event)
{
    switch (event->phase()) {
    case Qt::NoScrollPhase:
    case Qt::ScrollMomentum:
        return false;
    case Qt::ScrollBegin:
        m_axis = Axis::Undecided;
        m_total = QPointF();
        m_unscrolled = 0;
        m_scrolled = false;
        m_fired = false;
        m_window = window;
        return false;
    case Qt::ScrollEnd:
        m_axis = Axis::Undecided;
        return false;
    case Qt::ScrollUpdate:
        break;
    }
    if (window != m_window) {
        return false;
    }

    const QPointF delta = event->pixelDelta().isNull() ? QPointF(event->angleDelta()) / 120.0 * 20.0 : QPointF(event->pixelDelta());
    m_total += delta;
    if (m_axis == Axis::Undecided) {
        if (std::abs(m_total.x()) + std::abs(m_total.y()) < decideDistance) {
            return false;
        }
        m_axis = std::abs(m_total.x()) > 2 * std::abs(m_total.y()) ? Axis::Sideways : Axis::UpDown;
    }
    if (m_axis != Axis::Sideways) {
        return false;
    }

    // The sideways part alone, so a list that scrolls only up and down
    // does not take it.
    const QPoint pixel(event->pixelDelta().x(), 0);
    const QPoint angle(event->angleDelta().x(), 0);
    QWheelEvent sideways(event->position(), event->globalPosition(), pixel, angle, event->buttons(), event->modifiers(), event->phase(),
                         event->inverted(), event->source(), event->pointingDevice());
    sideways.setAccepted(false);
    m_delivering = true;
    QCoreApplication::sendEvent(window, &sideways);
    m_delivering = false;

    if (sideways.isAccepted()) {
        m_scrolled = true;
    } else {
        m_unscrolled += delta.x();
    }
    if (!m_scrolled && !m_fired && std::abs(m_unscrolled) >= turnDistance) {
        m_fired = true;
        // Fingers going right go back, as on a Mac, whichever way scrolling
        // is set to go: the delta follows the fingers only when the scroll
        // direction is inverted (scrolling follows your fingers in
        // Settings).
        const bool fingersRight = event->inverted() ? m_unscrolled > 0 : m_unscrolled < 0;
        click(window, event->position(), fingersRight);
    }
    return true;
}

void HistorySwipe::click(QQuickWindow *window, const QPointF &position, bool back)
{
    const Qt::MouseButton button = back ? Qt::BackButton : Qt::ForwardButton;
    const QPointF global = window->mapToGlobal(position);
    QMouseEvent press(QEvent::MouseButtonPress, position, position, global, button, button, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, position, global, button, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &release);
}
