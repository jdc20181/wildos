#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Apps that start when you sign in, as Windows' Settings > Apps > Startup
// lists them: every autostart entry an app or the user installed, each
// switched on or off. Turning one off writes a copy into
// ~/.config/autostart with Hidden=true, the XDG way, so the app's own file
// is never touched and turning it back on removes only that override.
// Desktop parts that are not apps (the shell, agents, helpers) are left out.
class StartupApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

public:
    explicit StartupApps(QObject *parent = nullptr);

    QVariantList apps() const { return m_apps; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setEnabled(const QString &file, bool on);

Q_SIGNALS:
    void appsChanged();

private:
    QVariantList m_apps;
};
