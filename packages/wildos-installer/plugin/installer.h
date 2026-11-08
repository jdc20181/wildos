#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QVariantList>

/* What the installer pages share.
 *
 * Calamares gives every page, the sidebar and the footer their own QML
 * engine, so a QML singleton would be one object per page. This one is a
 * single object for the whole process. It holds:
 *  - the configuration objects of steps whose answers another page shows
 *    (time zone, keyboard, finished),
 *  - the checks of the "Before we start" page, run by check-computer,
 *  - the disk password page's state and the recovery key, which never
 *    leaves this process except into the LUKS key slot and onto the USB
 *    stick or paper the person chooses.
 */
class Installer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QObject *welcome READ welcome WRITE setWelcome NOTIFY welcomeChanged)
    Q_PROPERTY(QObject *locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QObject *keyboard READ keyboard WRITE setKeyboard NOTIFY keyboardChanged)
    Q_PROPERTY(QObject *finished READ finished WRITE setFinished NOTIFY finishedChanged)

    Q_PROPERTY(bool checking READ checking NOTIFY checksChanged)
    Q_PROPERTY(QVariantList checks READ checks NOTIFY checksChanged)
    Q_PROPERTY(QString alongsideBlocked READ alongsideBlocked NOTIFY checksChanged)

    Q_PROPERTY(bool encryptionReady MEMBER m_encryptionReady NOTIFY encryptionReadyChanged)
    Q_PROPERTY(QString recoveryKey READ recoveryKey CONSTANT)
    Q_PROPERTY(bool keySaved MEMBER m_keySaved NOTIFY keySavedChanged)
    Q_PROPERTY(QVariantList drives READ drives NOTIFY drivesChanged)
    /// "install" when the USB menu started straight into the installer
    /// (wildos.mode=install), so there is no trial desktop behind it.
    Q_PROPERTY(QString startMode READ startMode CONSTANT)

public:
    static Installer *instance();
    static Installer *create(QQmlEngine *, QJSEngine *);

    QObject *welcome() const { return m_welcome; }
    QObject *locale() const { return m_locale; }
    QObject *keyboard() const { return m_keyboard; }
    QObject *finished() const { return m_finished; }
    void setWelcome(QObject *o);
    void setLocale(QObject *o);
    void setKeyboard(QObject *o);
    void setFinished(QObject *o);

    bool checking() const { return m_checking; }
    QVariantList checks() const { return m_checks; }
    QString alongsideBlocked() const { return m_alongsideBlocked; }
    /// Runs check-computer again; checksChanged when it is done.
    Q_INVOKABLE void runChecks();

    /// Calamares' reversible scrambling for secrets in global storage
    /// (KStringHandler::obscure), so they are not plain text in its logs.
    Q_INVOKABLE QString obscure(const QString &text) const;

    /// Six groups of five characters, from a cryptographic random source.
    QString recoveryKey() const { return m_recoveryKey; }
    QVariantList drives() const { return m_drives; }
    QString startMode() const;
    /// Lists USB drives the key can be saved on: removable, writable, with
    /// a file system Windows and Wild OS can both read.
    Q_INVOKABLE void refreshDrives();
    /// Saves the key as a text file on @p device; returns "" or what went wrong.
    Q_INVOKABLE QString saveKey(const QString &device);

Q_SIGNALS:
    void welcomeChanged();
    void localeChanged();
    void keyboardChanged();
    void finishedChanged();
    void checksChanged();
    void encryptionReadyChanged();
    void keySavedChanged();
    void drivesChanged();

private:
    Installer();

    QPointer<QObject> m_welcome;
    QPointer<QObject> m_locale;
    QPointer<QObject> m_keyboard;
    QPointer<QObject> m_finished;
    bool m_checking = false;
    QVariantList m_checks;
    QString m_alongsideBlocked;
    bool m_encryptionReady = false;
    QString m_recoveryKey;
    bool m_keySaved = false;
    QVariantList m_drives;
};
