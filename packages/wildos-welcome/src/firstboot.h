#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

#include "wifi.h"

/* The first boot setup session (wildos-welcome --setup): steps 1 to 3,
 * region and language, Wi-Fi and the account (FirstRegion, FirstWifi and
 * FirstAccount), and the end of step 7, which hands everything to the new
 * account and signs in to it.
 *
 * It runs as the locked wildos-setup account. The time zone goes through
 * systemd-timedated and Wi-Fi through NetworkManager, both allowed for that
 * account by polkit; the language, the account and the hand-over go through
 * the org.wildos.Setup1 service in wildos-setup.
 */
class FirstBoot : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Wifi *wifi READ wifi CONSTANT)

    // [{ value: "en_GB.UTF-8", text: "English (United Kingdom)" }], by name.
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY regionChanged)
    Q_PROPERTY(QString formats READ formats WRITE setFormats NOTIFY regionChanged)
    // Every zone, the way Windows lists them: "(UTC+00:00) London", by offset.
    Q_PROPERTY(QVariantList zones READ zones CONSTANT)
    Q_PROPERTY(QString timeZone READ timeZone WRITE setTimeZone NOTIFY regionChanged)
    // "installer" (as the installer left it), "language", "internet" or "chosen".
    Q_PROPERTY(QString zoneSource READ zoneSource NOTIFY regionChanged)
    // "How things will look": date, time, numbers, currency, measurements.
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY previewChanged)
    // "", "busy", "done" or "failed".
    Q_PROPERTY(QString lookupState READ lookupState NOTIFY lookupChanged)

    // DESKTOP- and seven random capital letters and digits, as Windows
    // names a new computer: never taken from the person's name, because
    // every network the computer joins sees it.
    Q_PROPERTY(QString suggestedComputerName READ suggestedComputerName CONSTANT)
    Q_PROPERTY(bool encrypted READ encrypted CONSTANT)
    // The pictures an account can start with: Wild OS's own, then the
    // photos that come with the desktop.
    Q_PROPERTY(QStringList pictures READ pictures CONSTANT)

    // "", "creating", "created", "finishing", "signing-in" or "failed",
    // with the reason in error.
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString firstName READ firstName NOTIFY stateChanged)

public:
    explicit FirstBoot(QObject *parent = nullptr);

    Wifi *wifi() const { return m_wifi; }
    QVariantList languages() const { return m_languages; }
    QString language() const { return m_language; }
    void setLanguage(const QString &language);
    QString formats() const { return m_formats; }
    void setFormats(const QString &formats);
    QVariantList zones() const { return m_zoneList; }
    QString timeZone() const { return m_timeZone; }
    void setTimeZone(const QString &zone);
    QString zoneSource() const { return m_zoneSource; }
    QVariantMap preview() const;
    QString lookupState() const { return m_lookupState; }
    QString suggestedComputerName() const { return m_computerName; }
    bool encrypted() const;
    QStringList pictures() const;
    QString state() const { return m_state; }
    QString error() const { return m_error; }
    QString firstName() const { return m_firstName; }

    /* Asks an online service where this internet address is, once, and
     * only when the person presses Find it for me. */
    Q_INVOKABLE void findTimeZone();
    /* Saves the language, formats and time zone; regionApplied says how. */
    Q_INVOKABLE void applyRegion();
    Q_INVOKABLE bool validComputerName(const QString &name) const;
    Q_INVOKABLE void createAccount(const QString &realName, const QString &computerName, const QString &password, bool autoLogin, const QString &hint,
                                   const QString &picture);
    /* Hands steps 4 to 7 (plan) to the new account and signs in to it: the
     * session ends there. */
    Q_INVOKABLE void finish(const QVariantMap &plan);
    // For a step that fails before the request is sent, so the page says so
    // instead of the button doing nothing.
    Q_INVOKABLE void fail(const QString &message);

Q_SIGNALS:
    void regionChanged();
    void previewChanged();
    void lookupChanged();
    void stateChanged();
    void regionApplied(bool ok, const QString &message);
    void accountCreated();

private:
    void readLocales();
    void readZones();
    void pickDefaults();
    void setState(const QString &state, const QString &error = {});
    void applyZone();

    Wifi *m_wifi;
    QVariantList m_languages;
    QVariantList m_zoneList;
    QHash<QString, QString> m_countryOfZone;
    QString m_installedLanguage;
    QString m_installedFormats;
    QString m_language;
    QString m_formats;
    QString m_timeZone;
    QString m_zoneSource;
    QString m_lookupState;
    QString m_state;
    QString m_error;
    QString m_firstName;
    QString m_computerName;
    QTimer m_clock;
};
