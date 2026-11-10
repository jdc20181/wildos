#pragma once

#include <QElapsedTimer>
#include <QIcon>
#include <QPainterPath>
#include <QPointF>
#include <QSizeF>
#include <QString>

#include <utility>

class QPainter;

// The window a Windows app opens in while Windows mode gets it ready
// (boards 9.11 to 9.13, WinAppLaunch*.dc.html): a title bar with the app's
// icon, name and the "Windows" chip, and the W from start-up drawing
// itself, a light running laps round it while Windows connects, then one
// full trace, the W filling with colour and the spark flashing as the app
// fades in. After 20 seconds it says why it is taking longer and offers a
// way out.
//
// The timings are the board's: its keyframes are percentages of a 9 s
// loop, evaluated here against the real moments (the click, the app being
// ready), so the sequence looks the same however long Windows takes.
class LoadingView
{
public:
    enum class Stage { Starting, AlmostReady };
    enum class Hit { None, Drag, Minimise, Maximise, Close, Cancel, Details, Retry };

    LoadingView();

    void start(const QString &appName, const QIcon &icon, bool reducedMotion);
    void setStage(Stage stage);
    // Why it is taking longer, in plain words, and how far along it is
    // (0 to 100) when there is anything to measure; -1 for no bar.
    void setReason(const QString &reason, int percent, const QString &progress);
    // The app is ready: the closing sequence starts now.
    void finish();
    // The app's window is here and only has to draw: from now on opening is
    // not slow, whatever the clock says (unless it already showed as slow).
    void arrive() { m_arrived = !slow(); }
    // The program ended without showing a window: the loading window says
    // so and offers Try again and Close (the slow state's layout).
    void setEnded();
    bool ended() const { return m_ended; }
    // The window it took was not the app's after all.
    void unarrive() { m_arrived = false; }

    bool finishing() const { return m_finishAt >= 0; }
    // The closing sequence has run: only the app is left.
    bool done() const;
    // Whether anything moves, so the window keeps asking for frames.
    bool animating() const;
    bool slow() const;
    // The window has finished fading in (wlwin), so its frame stays as it is.
    bool settled() const { return m_still || seconds() > 0.3; }
    // How much of the app shows through, 0 to 1.
    double appOpacity() const;

    // The frame, glow and title bar (background) and the W and its words
    // (foreground), so a caller can keep the first and redraw the second.
    void paint(QPainter &p, const QSizeF &size, bool background = true, bool foreground = true) const;
    // What changes while the light runs its laps: the mark, and the line
    // under it until opening turns slow.
    QRectF steadyArea(const QSizeF &size) const;
    Hit hitTest(const QPointF &at, const QSizeF &size) const;
    void setHover(Hit hit) { m_hover = hit; }
    Hit hover() const { return m_hover; }

private:
    double seconds() const;
    double introPercent() const;
    double finishPercent() const;
    void paintTitleBar(QPainter &p, const QSizeF &size, double chipOpacity) const;
    void paintMark(QPainter &p, const QPointF &origin, double x, double t) const;
    QRectF buttonRect(Hit which, const QSizeF &size) const;
    // The two buttons of the slow and ended states, with what each does.
    std::pair<std::pair<QString, Hit>, std::pair<QString, Hit>> buttons() const;

    QElapsedTimer m_clock;
    QString m_app;
    QIcon m_icon;
    bool m_still = false;
    Stage m_stage = Stage::Starting;
    double m_stageAt = 0;
    double m_finishAt = -1;
    bool m_arrived = false;
    bool m_ended = false;
    QString m_reason;
    int m_percent = -1;
    QString m_progress;
    Hit m_hover = Hit::None;
    QPainterPath m_w;
    QPainterPath m_spark;
    double m_length = 441;
    // The reason's height, measured once for the slow state's steady area.
    mutable QString m_measured;
    mutable double m_measuredHeight = 0;
};
