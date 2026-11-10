#pragma once

#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QQmlEngine>

// A desktop notification with one action, for news that matters when the
// window isn't in front: a remote PC that disconnected (RemoteWindowed.dc,
// "Work PC disconnected" with Reconnect).
class Notify : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Notify *instance();
    static Notify *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE void send(const QString &title, const QString &body, const QString &action, const QJSValue &onAction);

private Q_SLOTS:
    void invoked(uint id, const QString &key);

private:
    explicit Notify(QObject *parent = nullptr);

    QHash<uint, QJSValue> m_actions;
};
