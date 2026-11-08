#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

// What apps from the Store (Flatpaks) can reach, by kind, after Windows 11's
// Privacy and security > App permissions pages.
//
// Camera, microphone, files and internet are the app's sandbox permissions.
// A switch writes the user override file (~/.local/share/flatpak/overrides/
// <app>) exactly as the Store's permission switches do (wildos-store
// StoreApp::setPermission): only differences from what the app asked for,
// with the same tokens, so both always agree. Apps that ask through the
// desktop's camera or location prompts are remembered by the portal's
// permission store, and their switch changes that answer.
class AppAccess : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(int appCount READ appCount NOTIFY changed)

public:
    explicit AppAccess(QObject *parent = nullptr);

    QVariantMap summary() const { return m_summary; }
    int appCount() const { return m_apps.size(); }

    // kind is camera, microphone, location, files or network. Rows are
    // { id, name, icon, allowed, token } and, for files, { ..., items:
    // [{ token, label, allowed }] } with one item per folder asked for.
    Q_INVOKABLE QVariantList apps(const QString &kind) const;
    Q_INVOKABLE void set(const QString &app, const QString &token, bool allowed);
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void changed();

private:
    struct App {
        QString id;
        QString name;
        QString icon;
        QByteArray metadata;
    };

    QString overridePath(const QString &id) const;
    int state(const App &app, const QString &group, const QString &base) const;
    QVariantMap portal(const QString &table, const QString &id) const;
    void setPortal(const QString &table, const QString &id, const QString &app, const QStringList &value);

    QList<App> m_apps;
    QVariantMap m_camera;
    QVariantMap m_location;
    QVariantMap m_summary;
};
