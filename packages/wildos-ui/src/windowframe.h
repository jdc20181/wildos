#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSize>

class KWindowShadow;

// The parts of a window frame only the compositor can draw, for Wild OS
// apps that draw their own title bar: the soft drop shadow and the frosted
// blur behind the rounded glass. Moving and resizing go through
// QWindow::startSystemMove/startSystemResize, which QML calls directly.
//
// Based on wildos-settings/src/windowframe.cpp (the shadow is cut around
// the rounded corners so nothing darkens the translucent glass, updates
// are coalesced while resizing) plus wildos-files/src/windowframe.cpp
// (effects set again when the platform surface is recreated, none of
// it while maximised).
class WindowFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY changed)
    Q_PROPERTY(int radius READ radius WRITE setRadius NOTIFY changed)
    Q_PROPERTY(bool shadow READ shadow WRITE setShadow NOTIFY changed)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor NOTIFY changed)
    Q_PROPERTY(bool blur READ blur WRITE setBlur NOTIFY changed)
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY blurAvailableChanged)
    Q_PROPERTY(bool maximised READ maximised NOTIFY maximisedChanged)

public:
    explicit WindowFrame(QObject *parent = nullptr);
    ~WindowFrame() override;

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    int radius() const { return m_radius; }
    void setRadius(int radius);
    bool shadow() const { return m_shadowOn; }
    void setShadow(bool on);
    QColor shadowColor() const { return m_shadowColour; }
    void setShadowColor(const QColor &colour);
    bool blur() const { return m_blurOn; }
    void setBlur(bool on);
    bool blurAvailable() const { return m_blurAvailable; }
    bool maximised() const { return m_maximised; }

    // KWin loads and unloads its blur effect when transparency effects are
    // switched, and nothing announces it, so windows ask again when they
    // are activated.
    Q_INVOKABLE void recheck();
    Q_INVOKABLE void toggleMaximised();

Q_SIGNALS:
    void changed();
    void blurAvailableChanged();
    void maximisedChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void schedule();
    void apply();
    void applyShadow(bool full);
    void applyBlur(bool full);
    void dropShadow();

    QPointer<QQuickWindow> m_window;
    int m_radius = 14;
    bool m_shadowOn = true;
    // shadow-window: 0 30px 80px rgba(4,8,24,0.45).
    QColor m_shadowColour = QColor(4, 8, 24, 115);
    bool m_blurOn = true;
    bool m_blurAvailable = false;
    bool m_pending = false;
    bool m_maximised = false;
    KWindowShadow *m_shadow = nullptr;
    QSize m_blurSize;
    int m_blurRadius = -1;
};
