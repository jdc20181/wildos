#pragma once

#include <QObject>
#include <QQmlEngine>

class KModifierKeyInfo;

// Whether Caps Lock and Num Lock are on, for the indicator in the middle of
// the screen. KModifierKeyInfo asks the compositor (KWin's key state
// protocol on Wayland, XKB under X11), so it sees every keyboard, not just
// key presses in this process.
class KeyState : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool capsLock READ capsLock NOTIFY capsLockChanged)
    Q_PROPERTY(bool numLock READ numLock NOTIFY numLockChanged)

public:
    explicit KeyState(QObject *parent = nullptr);

    bool capsLock() const { return m_caps; }
    bool numLock() const { return m_num; }

Q_SIGNALS:
    void capsLockChanged();
    void numLockChanged();
    // Only real changes, never the state read at start-up.
    void toggled(bool capsLock, bool on);

private:
    KModifierKeyInfo *m_info;
    bool m_caps = false;
    bool m_num = false;
};
