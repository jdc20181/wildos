#pragma once

#include <KConfigWatcher>

#include <QObject>
#include <QQmlEngine>
#include <QString>

// Touchpad gestures, kept in wildosrc [Gestures]. KWin's wildos_gestures
// effect reads the three and four-finger swipes and three-finger drag, and
// Wild OS apps read the two-finger swipe between pages, each the moment it
// changes.
class Gestures : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool threeFingerDrag READ threeFingerDrag WRITE setThreeFingerDrag NOTIFY changed)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)

public:
    explicit Gestures(QObject *parent = nullptr);

    bool threeFingerDrag() const;
    void setThreeFingerDrag(bool on);
    bool isDefault() const;

    // gesture: ThreeUp, ThreeDown, ThreeSideways, FourUp, FourDown,
    // FourSideways or PageSwipe. action: taskview, desktop, desktops,
    // appgrid, notifications, history or nothing.
    Q_INVOKABLE QString action(const QString &gesture) const;
    Q_INVOKABLE void setAction(const QString &gesture, const QString &action);
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void changed();

private:
    KConfigGroup group() const;
    void write(const char *key, const QVariant &value);

    KConfigWatcher::Ptr m_watcher;
};
