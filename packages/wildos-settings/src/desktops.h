#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Virtual desktops, through KWin's org.kde.KWin.VirtualDesktopManager on
// /VirtualDesktopManager, the interface kcm_kwin_virtualdesktops and the
// pager use. KWin saves every change to kwinrc [Desktops] itself, and its
// signals keep this list current when desktops change elsewhere (Task
// View, a shortcut, another app).
class Desktops : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY changed)
    // { id, name, position } in order, position counting from 0.
    Q_PROPERTY(QVariantList desktops READ desktops NOTIFY changed)
    Q_PROPERTY(QString current READ current NOTIFY changed)
    // KWin's own limit (VirtualDesktopManager::maximum()).
    Q_PROPERTY(int maximum READ maximum CONSTANT)

public:
    explicit Desktops(QObject *parent = nullptr);

    bool available() const { return m_available; }
    QVariantList desktops() const { return m_desktops; }
    QString current() const { return m_current; }
    int maximum() const { return 20; }

    Q_INVOKABLE void add();
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void switchTo(const QString &id);

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void reload();

private:
    void call(const QString &method, const QVariantList &arguments);

    bool m_available = false;
    QVariantList m_desktops;
    QString m_current;
};
