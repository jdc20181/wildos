#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// The Windows apps page: every app Windows mode has a Start entry for, and
// whether it is open. They all share one Windows, whose CPU and memory the
// Processes page shows as one row, so there is nothing to show per app.
// Opening one goes through its Start entry; they are closed from their own
// windows, never ended from here, where unsaved work would be lost.
class WindowsApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

public:
    explicit WindowsApps(QObject *parent = nullptr);

    QVariantList apps() const { return m_apps; }

    Q_INVOKABLE void open(const QString &id);
    Q_INVOKABLE void openSettings();

Q_SIGNALS:
    void appsChanged();

private:
    void reload();

    QVariantList m_apps;
};
