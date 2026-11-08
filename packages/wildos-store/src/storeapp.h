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

    Q_INVOKABLE void install();
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
