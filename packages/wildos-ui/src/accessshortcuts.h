#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>

// The accessibility shortcuts on the lock and sign-in screens, which KDE's
// global shortcuts do not reach: the lock screen lets only a short list
// through (the magnifier among them) and the sign-in screen's KWin runs
// with none at all. Watching every key that comes to the screen's own
// windows, it catches
//
//   Super Alt S     screen reader          wildos-access toggle screen-reader
//   Super Alt H     high contrast          wildos-access toggle high-contrast
//   Shift x5        sticky keys            wildos-access toggle sticky-keys
//   Super Ctrl O    on-screen keyboard     keyboardRequested(), as each
//                                          screen has its own keyboard panel
//   Super + - 0     magnifier              wildos-access zoom, when
//                                          `magnifier` is set (sign-in
//                                          screen; the lock screen gets
//                                          these from KWin already)
//
// so the shortcuts Settings > Accessibility lists work before anyone has
// signed in, exactly as they do on the desktop.
class AccessShortcuts : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool magnifier MEMBER m_magnifier NOTIFY magnifierChanged)
    // Whether the on-screen keyboard was left on (wildosrc [Accessibility]
    // OnScreenKeyboard, which Settings writes and the sign-in screen gets a
    // copy of), so a screen can show its keyboard from the start.
    Q_PROPERTY(bool keyboardOn READ keyboardOn CONSTANT)

public:
    explicit AccessShortcuts(QObject *parent = nullptr);

    bool keyboardOn() const;

    // Starts what was left on and needs a process of its own (the screen
    // reader), for a screen with nobody else to do it.
    Q_INVOKABLE void restore() const;

Q_SIGNALS:
    void magnifierChanged();
    void keyboardRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void run(const QStringList &arguments) const;

    bool m_magnifier = false;
    bool m_shiftAlone = false;
    int m_shiftTaps = 0;
    QElapsedTimer m_lastShift;
};
