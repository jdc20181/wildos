#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Notifications from apps, after Windows 11: every app that can notify,
// sorted by name, with whether it may notify at all and, per app, banners,
// sound and the notification list.
//
// The apps are the ones KDE's notification settings list (an app with a
// KNotification events file, a desktop file that says it uses
// notifications, or an app the notification server has seen), less
// services that are not apps. Banners and the list are the notification
// server's per-app settings in plasmanotifyrc, [Applications][<desktop
// entry>] ShowPopups and ShowInHistory, written the way
// NotificationManager::Settings writes them so plasmashell picks them up
// at once. Sound exists only for apps with an events file: it is the Sound
// action of each of the app's events in ~/.config/<app>.notifyrc, which
// KNotification rereads when told to. Apps that send plain freedesktop
// notifications play their own sounds, if any, so they get no sound switch.
class AppNotifications : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

public:
    explicit AppNotifications(QObject *parent = nullptr);

    QVariantList apps() const { return m_apps; }

    Q_INVOKABLE void reload();
    // Off turns off banners, the list, badges and sound together; on turns
    // them all back to their defaults.
    Q_INVOKABLE void setEnabled(const QString &app, bool on);
    Q_INVOKABLE void setBanners(const QString &app, bool on);
    Q_INVOKABLE void setHistory(const QString &app, bool on);
    Q_INVOKABLE void setSound(const QString &app, bool on);

Q_SIGNALS:
    void appsChanged();

private:
    struct App {
        QString id;
        QString name;
        QString icon;
        // The events file's name without .notifyrc, for apps that have one.
        QString notifyRc;
    };

    QString notifyRcFor(const QString &app) const;
    void writeBehaviour(const QString &app, const QString &key, bool on);
    void writeSound(const QString &notifyRc, bool on);
    void publish();

    QList<App> m_list;
    QVariantList m_apps;
};
