#pragma once

#include <KConfigWatcher>

#include <QObject>
#include <QQmlEngine>

// Keep awake from a hot corner, for the cup in the taskbar corner. KWin's
// hot corners (org.wildos.hotcorners) hold the block on dimming, sleep and
// locking and keep wildosrc [KeepAwake] Active in step with it; the cup shows
// while it is true, and clicking it sets it false, which KWin sees and lets
// go of the block.
class KeepAwake : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit KeepAwake(QObject *parent = nullptr);

    bool active() const { return m_active; }
    Q_INVOKABLE void turnOff();

Q_SIGNALS:
    void activeChanged();

private:
    void read();

    KConfigWatcher::Ptr m_watcher;
    bool m_active = false;
};
