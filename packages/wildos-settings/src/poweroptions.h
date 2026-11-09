#pragma once

#include <QObject>
#include <QQmlEngine>

// What this computer can do with its lid and power button, so More power
// settings only offers choices that work here: a lid row only on a laptop,
// sleep and hibernate only where the system supports them. The answers
// come from the same places powerdevil's own settings module asks: UPower
// for the lid, powerdevil's org.freedesktop.PowerManagement for sleep.
class PowerOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool lidPresent READ lidPresent NOTIFY changed)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY changed)
    Q_PROPERTY(bool canHibernate READ canHibernate NOTIFY changed)

public:
    explicit PowerOptions(QObject *parent = nullptr);

    bool lidPresent() const { return m_lidPresent; }
    bool canSuspend() const { return m_canSuspend; }
    bool canHibernate() const { return m_canHibernate; }

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    void ask(const QString &method, bool *target);

    bool m_lidPresent = false;
    bool m_canSuspend = false;
    bool m_canHibernate = false;
};
