#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

class Catalog;

// One app as QML sees it. The data comes from the Catalog's record and is
// read on demand, so a catalogue reload updates every open page.
class StoreApp : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Apps come from Catalog.app()")

    Q_PROPERTY(QString key READ key CONSTANT)
    Q_PROPERTY(bool valid READ isValid NOTIFY dataChanged)
    Q_PROPERTY(QString appstreamId READ appstreamId NOTIFY dataChanged)
    Q_PROPERTY(QString name READ name NOTIFY stateChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY dataChanged)
    Q_PROPERTY(QString tagline READ tagline NOTIFY dataChanged)
    Q_PROPERTY(QString byline READ byline NOTIFY dataChanged)
    Q_PROPERTY(QString description READ description NOTIFY dataChanged)
    Q_PROPERTY(QString developer READ developer NOTIFY dataChanged)
    Q_PROPERTY(QString iconSource READ iconSource NOTIFY stateChanged)
    Q_PROPERTY(QVariantList screenshots READ screenshots NOTIFY dataChanged)
    Q_PROPERTY(QString categoryLabel READ categoryLabel NOTIFY dataChanged)
    Q_PROPERTY(bool openSource READ openSource NOTIFY dataChanged)
    Q_PROPERTY(QString licence READ licence NOTIFY dataChanged)
    Q_PROPERTY(QString homepage READ homepage NOTIFY dataChanged)
    Q_PROPERTY(QString insteadOf READ insteadOf NOTIFY dataChanged)
    Q_PROPERTY(QString cardNote READ cardNote NOTIFY dataChanged)
    Q_PROPERTY(QString chip READ chip NOTIFY dataChanged)
    Q_PROPERTY(QString swapTitle READ swapTitle NOTIFY dataChanged)
    Q_PROPERTY(QString swapText READ swapText NOTIFY dataChanged)
    Q_PROPERTY(bool hasFlatpak READ hasFlatpak NOTIFY stateChanged)
    Q_PROPERTY(bool hasApt READ hasApt NOTIFY dataChanged)

    Q_PROPERTY(QString source READ source NOTIFY stateChanged)
    Q_PROPERTY(bool installed READ isInstalled NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(bool queued READ isQueued NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString action READ action NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString version READ version NOTIFY stateChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY stateChanged)
    Q_PROPERTY(bool canOpen READ canOpen NOTIFY stateChanged)
    Q_PROPERTY(bool inStart READ inStart NOTIFY stateChanged)
    Q_PROPERTY(QVariantList permissions READ permissions NOTIFY stateChanged)
    Q_PROPERTY(bool permissionsUnknown READ permissionsUnknown NOTIFY stateChanged)
    // The quiet line under an app's button: "Downloading 214 MB of 480 MB",
    // then "Installing", then nothing.
    Q_PROPERTY(QString progressText READ progressText NOTIFY stateChanged)
    // "Includes the KDE base, shared with other apps", when installing this
    // app would also bring the base its kind of app runs on.
    Q_PROPERTY(QString sharedNote READ sharedNote NOTIFY stateChanged)
    // Whether the step running now can be stopped part way.
    Q_PROPERTY(bool cancellable READ cancellable NOTIFY stateChanged)

    // The Store button's state, as StoreStates.dc.html draws them: get,
    // waiting, preparing, downloading, installing, removing, open,
    // installed, update, paused, retry or nospace.
    Q_PROPERTY(QString buttonState READ buttonState NOTIFY stateChanged)
    // The short line under a button in lists ("Needs 720 MB free",
    // "Download failed"), empty when there is nothing to say.
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY stateChanged)
    // "412 of 1,080 MB" and "About a minute left", while downloading.
    Q_PROPERTY(QString bytesText READ bytesText NOTIFY stateChanged)
    Q_PROPERTY(QString timeLeftText READ timeLeftText NOTIFY stateChanged)
    // What installing downloads ("312 MB"), or what an update downloads.
    Q_PROPERTY(QString downloadText READ downloadText NOTIFY stateChanged)
    Q_PROPERTY(QString updateSizeText READ updateSizeText NOTIFY stateChanged)
    // The newest release's notes, as one plain line.
    Q_PROPERTY(QString releaseNote READ releaseNote NOTIFY dataChanged)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY dataChanged)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY stateChanged)
    // Sandboxed is true of every Flathub app. Checked only when Flathub
    // has verified who publishes it, or Wild OS picked it.
    Q_PROPERTY(bool sandboxed READ sandboxed NOTIFY stateChanged)
    Q_PROPERTY(bool checked READ isChecked NOTIFY dataChanged)
    // From the permissions in the app's Flatpak metadata, each entry an
    // {icon, label} map. Empty until the metadata is in.
    Q_PROPERTY(QVariantList canReach READ canReach NOTIFY stateChanged)
    Q_PROPERTY(QVariantList cantReach READ cantReach NOTIFY stateChanged)
    Q_PROPERTY(bool worksOffline READ worksOffline NOTIFY stateChanged)
    Q_PROPERTY(bool reachKnown READ reachKnown NOTIFY stateChanged)
    // The choices before installing: only what the app asks for, each
    // {token, icon, label, detail, allowed, required}.
    Q_PROPERTY(QVariantList consentItems READ consentItems NOTIFY stateChanged)

public:
    StoreApp(Catalog *catalog, const QString &key);

    QString key() const { return m_key; }
    bool isValid() const;
    QString appstreamId() const;
    QString name() const;
    QString summary() const;
    QString tagline() const;
    QString byline() const;
    QString description() const;
    QString developer() const;
    QString iconSource() const;
    QVariantList screenshots() const;
    QString categoryLabel() const;
    bool openSource() const;
    QString licence() const;
    QString homepage() const;
    QString insteadOf() const;
    QString cardNote() const;
    QString chip() const;
    QString swapTitle() const;
    QString swapText() const;
    bool hasFlatpak() const;
    bool hasApt() const;

    QString source() const;
    bool isInstalled() const;
    bool updateAvailable() const;
    bool isBusy() const;
    bool isQueued() const;
    int progress() const;
    QString phase() const;
    QString action() const;
    QString error() const;
    QString version() const;
    QString sizeText() const;
    bool canOpen() const;
    bool inStart() const;
    QVariantList permissions() const;
    bool permissionsUnknown() const;
    QString progressText() const;
    QString sharedNote() const;
    bool cancellable() const;
    QString buttonState() const;
    QString statusLine() const;
    QString bytesText() const;
    QString timeLeftText() const;
    QString downloadText() const;
    QString updateSizeText() const;
    QString releaseNote() const;
    QString newVersion() const;
    QString updatedText() const;
    bool sandboxed() const;
    bool isChecked() const;
    QVariantList canReach() const;
    QVariantList cantReach() const;
    bool worksOffline() const;
    bool reachKnown() const;
    QVariantList consentItems() const;
    // Bytes short of what installing needs, or 0.
    qint64 spaceShort() const;

    // Launchers are looked up once each, until something is installed or
    // removed or KDE's service cache is rebuilt.
    static void forgetServices();

    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();
    // The stop square: stops the download, keeping what it fetched.
    Q_INVOKABLE void pause();
    // Writes the choices made before installing as the app's Flatpak
    // overrides, then installs. choices maps a consent token to on or off.
    Q_INVOKABLE void installWith(const QVariantMap &choices);
    Q_INVOKABLE void remove();
    Q_INVOKABLE void update();
    Q_INVOKABLE bool open();
    Q_INVOKABLE void useSource(const QString &source);
    Q_INVOKABLE void loadDetails();
    Q_INVOKABLE void setPermission(const QString &token, bool allowed);
    Q_INVOKABLE void clearError();

Q_SIGNALS:
    void dataChanged();
    void stateChanged();

private:
    QString desktopId() const;
    QString serviceName() const;
    QString flatpakOverridePath() const;

    Catalog *m_catalog;
    QString m_key;
};
