#pragma once

#include <QDBusContext>
#include <QObject>
#include <QTimer>

/* org.wildos.Setup1, the root half of the first boot setup session.
 *
 * The setup pages run as the locked system user wildos-setup. Everything
 * they cannot do as that user goes through here: making the one account,
 * naming the computer, and handing over to the new account at the end.
 * Each call is checked with polkit (actions in org.wildos.setup.policy,
 * granted only to the setup user in 10-wildos-setup.rules) and refused once
 * the first boot marker is gone, so nothing here works after setup.
 */
class SetupService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.wildos.Setup1")

public:
    explicit SetupService(QObject *parent = nullptr);

public Q_SLOTS:
    /* Makes the account and names the computer. Returns the sign-in name,
     * made from the first word of realName. Can be called again (after a
     * failure, or Back and Continue): the account from the earlier call is
     * removed first. */
    QString CreateAccount(const QString &realName, const QString &password, const QString &hostname, bool autoLogin, const QString &hint,
                          const QString &picture);

    /* Sets the display language (LANG) and the formats (dates, numbers,
     * money, paper, measurements), making either locale first if the
     * computer does not have it yet. */
    bool SetLocale(const QString &language, const QString &formats);

    /* The last five characters of the Windows licence key built into the
     * computer's firmware, or empty when it has none. */
    QString FirmwareLicence();

    /* Ends setup: gives the new account the choices from steps 4 to 7
     * (plan, JSON, applied by its own welcome at its first sign-in), deletes
     * the marker and restarts the sign-in screen, which signs straight in to
     * the new account once. Replies before the restart ends the caller's
     * session. */
    void Finish(const QString &plan);

private:
    bool authorised(const QString &action);
    bool refuse(const QString &name, const QString &sentence);
    void removeEarlierAccount();
    void handOver(const QString &userName, uint uid);
    void touch();

    QTimer m_idle;
    bool m_finishing = false;
};
