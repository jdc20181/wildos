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
//
// "Everything an app can reach" lists the whole sandbox of one app, as
// "flatpak info --show-permissions" would, and changes any of it through
// the same override file: the same tokens and negations "flatpak override
// --user" writes, so the command line, the Store and Settings all agree.
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
    // token is "<Context key>:<value>" (shared, sockets, devices, features,
    // filesystems), "session-bus:<name>", "system-bus:<name>", "env:<NAME>",
    // or camera and location. grant is what an allowed entry is written as
    // when it differs from the app's own: a folder with its ":ro", a bus
    // name's talk or own, a variable's value.
    Q_INVOKABLE void set(const QString &app, const QString &token, bool allowed, const QString &grant = QString());
    Q_INVOKABLE void reload();

    // Every app from the Store: { id, name, icon, changed }.
    Q_INVOKABLE QVariantList everyApp() const;
    // One app's whole sandbox as sections { key, title, items: [{ token,
    // label, detail, allowed, grant, changed }] }.
    Q_INVOKABLE QVariantList details(const QString &app) const;
    // Gives an app something it did not ask for. kind is folder, bus or env;
    // returns why it was refused, or nothing.
    Q_INVOKABLE QString add(const QString &app, const QString &kind, const QString &value, const QString &extra);
    // Deletes the app's override file, so it gets exactly what it asked for.
    Q_INVOKABLE void reset(const QString &app);

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
    const App *find(const QString &id) const;
    int state(const App &app, const QString &group, const QString &base) const;
    QVariantMap portal(const QString &table, const QString &id) const;
    void setPortal(const QString &table, const QString &id, const QString &app, const QStringList &value);

    QList<App> m_apps;
    QVariantMap m_camera;
    QVariantMap m_location;
    QVariantMap m_summary;
};
