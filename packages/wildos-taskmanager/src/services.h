#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

// The Services page: the system's and this account's systemd services with
// plain names, whether each is running and how it starts, and Start, Stop
// and Restart. System services are changed through systemd itself, which
// asks for an administrator's password through polkit; this account's own
// go to the user's systemd and need none. By default only services a person
// may want to touch are listed; "Show all" lists every one.
class Services : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged)
    Q_PROPERTY(bool showAll READ showAll WRITE setShowAll NOTIFY showAllChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit Services(QObject *parent = nullptr);

    QVariantList services() const { return m_visible; }
    bool showAll() const { return m_showAll; }
    void setShowAll(bool all);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    // Only polled while the page is open.
    bool active() const { return m_timer.isActive(); }
    void setActive(bool active);

    // "start", "stop" or "restart" on a unit; "user" picks this account's
    // systemd.
    Q_INVOKABLE void act(const QString &unit, bool user, const QString &action);
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void servicesChanged();
    void showAllChanged();
    void searchChanged();
    void activeChanged();
    void failed(const QString &message);

private:
    void filter();
    QVariantList list(bool user) const;

    QTimer m_timer;
    QVariantList m_all;
    QVariantList m_visible;
    bool m_showAll = false;
    QString m_search;
};
