#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRectF>

// How a system dialog's window sits on the desktop. Every one of them is a
// layer surface on Wayland, which no app window can cover, draw over or
// imitate, because only the shell may create them:
//
//   overlay  the whole screen, above everything including panels and full
//            screen apps, holding the keyboard until it closes (the
//            permission prompt and Ctrl+Alt+Del)
//   prompt   centred above windows, taking the keyboard only when clicked
//            (battery, restart, Windows mode)
//   backdrop the same dimmed, blurred cover for the other screens, without
//            the keyboard
//   dialog   an ordinary window attached to another app's window (not
//            responding); `parentHandle` is that window's xdg-foreign handle
//
// On X11 the same roles become a frameless window kept on top. The
// compositor blurs what is behind `blurRect` (the whole window when it is
// empty), so the glass reads as glass.
class Surface : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY changed)
    Q_PROPERTY(QString role READ role WRITE setRole NOTIFY changed)
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY changed)
    Q_PROPERTY(bool blur READ blur WRITE setBlur NOTIFY changed)
    Q_PROPERTY(QRectF blurRect READ blurRect WRITE setBlurRect NOTIFY changed)
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius NOTIFY changed)
    Q_PROPERTY(QString parentHandle READ parentHandle WRITE setParentHandle NOTIFY changed)
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY blurAvailableChanged)
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)

public:
    explicit Surface(QObject *parent = nullptr);

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    QString role() const { return m_role; }
    void setRole(const QString &role);
    QString scope() const { return m_scope; }
    void setScope(const QString &scope);
    bool blur() const { return m_blur; }
    void setBlur(bool on);
    QRectF blurRect() const { return m_blurRect; }
    void setBlurRect(const QRectF &rect);
    qreal blurRadius() const { return m_blurRadius; }
    void setBlurRadius(qreal radius);
    QString parentHandle() const { return m_parentHandle; }
    void setParentHandle(const QString &handle);
    bool blurAvailable() const { return m_blurAvailable; }
    bool shown() const { return m_window && m_window->isVisible(); }

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

    // True on a Wayland session with layer-shell, where the roles above
    // hold; false on X11 and in the preview.
    static bool layerShell();

Q_SIGNALS:
    void changed();
    void blurAvailableChanged();
    void shownChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool fullScreen() const { return m_role == QLatin1String("overlay") || m_role == QLatin1String("backdrop"); }
    void prepare();
    void applyBlur();

    QPointer<QQuickWindow> m_window;
    QString m_role = QStringLiteral("prompt");
    QString m_scope = QStringLiteral("wildos-dialog");
    bool m_blur = true;
    QRectF m_blurRect;
    qreal m_blurRadius = 0;
    QString m_parentHandle;
    bool m_blurAvailable = false;
    bool m_prepared = false;
};
