#pragma once

#include <KConfigWatcher>

#include <QObject>
#include <QPointF>
#include <QPointer>

class QQuickWindow;
class QWheelEvent;

// Two fingers swiped left or right on a touchpad go back and forward, as on
// a Mac, in every Wild OS app: the swipe becomes the mouse's back or
// forward button, which Files, Settings, Store and Music already follow.
//
// The touchpad's scroll reaches the window first; while the swipe is
// mostly sideways it goes on with the up and down movement taken out, so a
// list that only scrolls up and down leaves it alone, and it turns into a
// page turn only if nothing in the window scrolled sideways with it. A wide
// table or an image still scrolls sideways as before.
//
// wildosrc [Gestures] PageSwipe=nothing turns it off (Settings > Bluetooth
// and devices > Touchpad > Gestures).
class HistorySwipe : public QObject
{
    Q_OBJECT

public:
    // One per app, made by the first Theme.
    static void install();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit HistorySwipe(QObject *parent);
    void readConfig();
    bool wheel(QQuickWindow *window, QWheelEvent *event);
    void click(QQuickWindow *window, const QPointF &position, bool back);

    KConfigWatcher::Ptr m_watcher;
    bool m_enabled = true;
    bool m_delivering = false;

    // The scroll under the fingers, from its ScrollBegin.
    enum class Axis {
        Undecided,
        Sideways,
        UpDown,
    };
    Axis m_axis = Axis::Undecided;
    QPointF m_total;
    qreal m_unscrolled = 0;
    bool m_scrolled = false;
    bool m_fired = false;
    QPointer<QQuickWindow> m_window;
};
