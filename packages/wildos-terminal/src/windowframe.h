#pragma once

#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QWindow>

class KWindowShadow;

// What the Aurorae decoration gives every other window and a frameless one
// has to ask the compositor for itself: the design's soft shadow and the
// frosted blur behind the glass. The same numbers Files and Settings use,
// so the three windows sit on the desktop alike.
class WindowFrame : public QObject
{
    Q_OBJECT

public:
    explicit WindowFrame(QWindow *window, QObject *parent = nullptr);
    ~WindowFrame() override;

    void setRadius(int radius);
    void setShadow(bool on);
    // The part of the window to blur, in window coordinates.
    void setBlurRegion(const QRegion &region);
    bool blurAvailable() const { return m_blurAvailable; }
    // KWin loads and unloads its blur effect when transparency effects are
    // switched, and nothing announces it, so the window asks again when it
    // is activated. True when the answer changed.
    bool recheck();

    static QRegion roundedRegion(const QSize &size, int radius, bool roundBottom = true);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void schedule();
    void apply();
    void applyShadow();

    QPointer<QWindow> m_window;
    int m_radius = 14;
    bool m_shadowOn = true;
    bool m_blurAvailable = false;
    bool m_pending = false;
    KWindowShadow *m_shadow = nullptr;
    QRegion m_blurRegion;
    QRegion m_appliedBlur;
    bool m_blurApplied = false;
};
