#pragma once

#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QObject>
#include <QRect>
#include <QString>

// Takes the pictures. On Wayland only the compositor can see the screen,
// so this asks KWin through its ScreenShot2 D-Bus interface (the same one
// Spectacle uses), which KWin grants to programs whose desktop file lists
// org.kde.KWin.ScreenShot2 in X-KDE-DBUS-Restricted-Interfaces. Under X11,
// or when KWin's screenshot effect is off, it falls back to Qt's own grab
// of the root window.
class Grabber : public QObject
{
    Q_OBJECT

public:
    explicit Grabber(QObject *parent = nullptr);

    // Every screen as it looks now, keyed by screen name, at the screen's
    // own resolution (devicePixelRatio set to its scale).
    QHash<QString, QImage> grabScreens();
    // One window on its own, without what covers it, by KWin's id for it.
    QImage grabWindow(const QString &id);

    // The windows on the current desktop, bottom to top, as KWin knows
    // them: {id, x, y, width, height, caption}. Asked of KWin through a
    // one-off script that calls back on D-Bus; finished when they arrive.
    void requestWindows();

Q_SIGNALS:
    void windowsArrived(const QJsonArray &windows);

private:
    QImage callKWin(const QString &method, const QVariantList &arguments);
    bool m_kwinWorks = true;
};

// The receiving end of the window list script.
class WindowListReceiver : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Screenshot")

public:
    explicit WindowListReceiver(Grabber *grabber);

public Q_SLOTS:
    Q_SCRIPTABLE void setWindows(const QString &json);

private:
    Grabber *m_grabber;
};
