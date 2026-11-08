#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QQuickWindow;
class Chooser;

// org.freedesktop.impl.portal.FileChooser: the Open and Save dialog every
// app reaches through xdg-desktop-portal (Flatpak apps, GTK and Qt apps
// told to use the portal, browsers). Each request is answered when the
// dialog closes; the frontend can end it early through the Request object
// at the handle path.
//
// Windows apps under Wine do not come here: Wine draws its own comdlg32
// dialogs and has no portal support. Reaching this dialog needs a native
// comdlg32 override (GetOpenFileNameW, GetSaveFileNameW and the
// IFileDialog classes) that calls org.freedesktop.portal.FileChooser,
// loaded through the WINEDLLOVERRIDES list App Bridge already sets for
// each app (wildos-appbridge/src/winapps.cpp), and a portal route out of
// App Bridge's sandbox.
class FileChooserPortal : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit FileChooserPortal(QObject *parent = nullptr);
    uint version() const { return 4; }

public Q_SLOTS:
    uint OpenFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                  const QVariantMap &options, QVariantMap &results);
    uint SaveFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                  const QVariantMap &options, QVariantMap &results);
    uint SaveFiles(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                   const QVariantMap &options, QVariantMap &results);

private:
    void start(int mode, const QDBusObjectPath &handle, const QString &appId, const QString &parentWindow, const QString &title,
               const QVariantMap &options);
};

// org.freedesktop.impl.portal.Request at a request's handle: Close ends the
// dialog without a choice.
class PortalRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    PortalRequest(const QDBusObjectPath &handle, Chooser *chooser, QObject *parent);
    ~PortalRequest() override;

public Q_SLOTS:
    void Close();

private:
    QString m_path;
    QPointer<Chooser> m_chooser;
};
