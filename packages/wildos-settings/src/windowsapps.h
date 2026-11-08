#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// What the Windows apps page knows about the App Bridge: whether it is
// installed, its two settings, and the Windows apps it has set up. Each app
// lives in its own Wine prefix under ~/.local/share/wildos/windows-apps/<name>
// and is started from a launcher whose Exec line points into that prefix.
//
// The settings are written to wildos-appbridgerc for the bridge to read:
//   [General] Enabled=true|false
//             StartingPermissions=documents-offline|documents-online|own-offline
// An app's own permissions are read from app.conf in its prefix, if the
// bridge wrote one: [Permissions] Documents=, Printer=, Internet= (bools).
class WindowsApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool installed READ installed CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY settingsChanged)
    Q_PROPERTY(QString startingPermissions READ startingPermissions NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

public:
    explicit WindowsApps(QObject *parent = nullptr);

    bool installed() const;
    bool enabled() const;
    QString startingPermissions() const;
    QVariantList apps() const { return m_apps; }

    Q_INVOKABLE void setEnabled(bool on);
    Q_INVOKABLE void setStartingPermissions(const QString &value);
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool uninstall(const QString &name);

Q_SIGNALS:
    void settingsChanged();
    void appsChanged();

private:
    static QString root();
    void watch();

    QVariantList m_apps;
    QFileSystemWatcher m_watcher;
};
