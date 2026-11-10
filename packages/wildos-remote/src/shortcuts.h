#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

struct zwp_keyboard_shortcuts_inhibit_manager_v1;
struct zwp_keyboard_shortcuts_inhibitor_v1;

// "Send all shortcuts there" (RemoteSession.dc): while on, the desktop's own
// shortcuts (Alt+Tab, the Windows key, Ctrl+Alt+Del's neighbours) reach the
// window instead of Wild OS, through Wayland's keyboard shortcuts inhibit
// protocol, which KWin offers. Only while the window has the keyboard.
class ShortcutInhibitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    // The compositor agreed; false when it can't (not KWin, or the
    // protocol is missing).
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit ShortcutInhibitor(QObject *parent = nullptr);
    ~ShortcutInhibitor() override;

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool active() const { return m_active; }
    bool available() const;

Q_SIGNALS:
    void changed();
    void activeChanged();

private:
    void apply();
    void release();
    static zwp_keyboard_shortcuts_inhibit_manager_v1 *manager();

    QPointer<QQuickWindow> m_window;
    bool m_enabled = false;
    bool m_active = false;
    zwp_keyboard_shortcuts_inhibitor_v1 *m_inhibitor = nullptr;
};
