#pragma once

#include <QObject>
#include <QQmlEngine>

class QFileSystemWatcher;

// "Steam and 32-bit apps", in Privacy and security. Wild OS starts with the
// kernel's 32-bit layer off (ia32_emulation=0); turning it on is a boot
// option written by settings-32bit, so what was chosen and what the running
// kernel does can differ until the next start-up. Strict and Maximum
// protection keep it off, as they do hyper-threading.
class Compat32 : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)
    // What was chosen; it applies from the next start-up.
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    // What the running kernel does.
    Q_PROPERTY(bool active READ active CONSTANT)
    // Strict or Maximum protection, which keep it off.
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    // A change is waiting for the password or being written.
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    // Steam from Flathub, which the performance overlay works with.
    Q_PROPERTY(bool steamInstalled READ steamInstalled NOTIFY changed)
    // "Show a performance overlay in games" (settings-game-overlay).
    Q_PROPERTY(bool overlay READ overlay NOTIFY changed)
    Q_PROPERTY(bool overlayBusy READ overlayBusy NOTIFY changed)

public:
    explicit Compat32(QObject *parent = nullptr);

    bool available() const;
    bool enabled() const;
    bool active() const;
    bool locked() const;
    bool busy() const { return m_busy; }
    bool steamInstalled() const;
    bool overlay() const;
    bool overlayBusy() const { return m_overlayBusy; }

    Q_INVOKABLE void setEnabled(bool on);
    Q_INVOKABLE void setOverlay(bool on);

Q_SIGNALS:
    void changed();
    void failed(const QString &message);

private:
    QFileSystemWatcher *m_watcher = nullptr;
    bool m_busy = false;
    bool m_overlayBusy = false;
};
