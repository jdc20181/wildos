#pragma once

#include <QObject>
#include <QQmlEngine>

// Old app compatibility: how much older (X11) apps, which run through
// Xwayland, may see of what you type into other apps. Some need it for a
// global shortcut such as push to talk.
//
// kwinrc [Xwayland] XwaylandEavesdrops is one of None, Modifiers,
// Combinations or All (KWin's kwin.kcfg; KConfigSkeleton stores an enum by
// its choice name) and XwaylandEavesdropsMouse adds mouse buttons, the keys
// KDE's own Legacy X11 App Support module writes. KWin reads them again on
// org.kde.KWin.reconfigure. Wild OS ships None (wildos-branding kwinrc).
class OlderApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 0 None, 1 Modifiers, 2 Combinations, 3 All.
    Q_PROPERTY(int keys READ keys WRITE setKeys NOTIFY changed)
    Q_PROPERTY(bool mouse READ mouse WRITE setMouse NOTIFY changed)

public:
    explicit OlderApps(QObject *parent = nullptr);

    int keys() const;
    void setKeys(int level);
    bool mouse() const;
    void setMouse(bool on);

Q_SIGNALS:
    void changed();

private:
    void reconfigure();
};
