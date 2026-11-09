#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Activity history, after Windows 11's Privacy and security > Activity
// history: whether this computer remembers the files you open (for Recent
// in Start and Files), for how long, apps it never remembers, and clearing
// it.
//
// The settings are KDE's recent files module's own (plasma-desktop
// kcms/recentFiles): kactivitymanagerd-pluginsrc
// [Plugin-org.kde.ActivityManager.Resources.Scoring] what-to-remember (0
// every app, 1 all but blocked-applications, 2 none), keep-history-for in
// months (0 is forever) and blocked-applications, plus kactivitymanagerdrc
// [Plugins] org.kde.ActivityManager.ResourceScoringEnabled. kactivitymanagerd
// watches both files, so a change applies at once. KIO's own recent
// documents list (recently-used.xbel) follows kdeglobals [RecentDocuments]
// UseRecent and is cleared with the rest.
class ActivityHistory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY changed)
    Q_PROPERTY(int keepMonths READ keepMonths WRITE setKeepMonths NOTIFY changed)
    Q_PROPERTY(QVariantList blocked READ blocked NOTIFY changed)
    Q_PROPERTY(bool clearing READ clearing NOTIFY clearingChanged)

public:
    explicit ActivityHistory(QObject *parent = nullptr);

    bool remember() const;
    void setRemember(bool on);
    int keepMonths() const;
    void setKeepMonths(int months);
    // { id, name, icon } by desktop file name, as kactivitymanagerd records
    // the app that opened a file.
    QVariantList blocked() const;
    bool clearing() const { return m_clearing; }

    // Apps from Start that are not blocked yet, for choosing one.
    Q_INVOKABLE QVariantList apps() const;
    Q_INVOKABLE void block(const QString &app);
    Q_INVOKABLE void unblock(const QString &app);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void changed();
    void clearingChanged();
    void cleared();
    void failed(const QString &reason);

private:
    QStringList blockedNames() const;
    void writeMode(bool remember, const QStringList &blocked);

    bool m_clearing = false;
};
