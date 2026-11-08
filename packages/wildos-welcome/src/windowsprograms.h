#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

/* Step 7, Your Windows programs (FirstWinApps.dc.html).
 *
 * Reads what was installed on the Windows drive found in step 5, which is
 * only ever mounted read-only: the Uninstall keys of the SOFTWARE registry
 * hive (with WOW6432Node, for 32-bit programs) and of the person's own
 * NTUSER.DAT, read with hivex, plus the folders in Program Files for
 * anything the registry does not list. Each program is matched against the
 * App Bridge's list (alternatives.json) and Wild OS's additions
 * (windows-programs.json) and put in one of these kinds:
 *
 *   desktop      runs on the Wild OS desktop: its Store replacement, or
 *                its own Windows installer, fetched fresh, through the App
 *                Bridge's Windows app support
 *   windows      needs Windows mode, the background tier with a real copy
 *                of Windows, which needs the person's own licence
 *   unsupported  cannot run on Wild OS, with the App Bridge's reason
 *   unknown      not on either list, so Wild OS has not tried it
 *
 * Nothing is fetched or installed here: plan() hands the ticked programs to
 * the account's own background job (Background::runPrograms).
 */
class WindowsPrograms : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "", "scanning", "done" or "none" (no Windows drive to read).
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // { key, name, detail, kind, action ("store", "download", "windows" or
    // ""), selectable, checked }
    Q_PROPERTY(QVariantList programs READ programs NOTIFY changed)
    // False when the registry could not be read and only Program Files was.
    Q_PROPERTY(bool registryRead READ registryRead NOTIFY changed)
    Q_PROPERTY(QStringList windowsModeNames READ windowsModeNames NOTIFY changed)
    Q_PROPERTY(int downloadCount READ downloadCount NOTIFY changed)

    // "", "checking", "firmware", "registry" or "none"
    Q_PROPERTY(QString licence READ licence NOTIFY licenceChanged)
    Q_PROPERTY(QString licenceText READ licenceText NOTIFY licenceChanged)

public:
    explicit WindowsPrograms(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QVariantList programs() const { return m_programs; }
    bool registryRead() const { return m_registryRead; }
    QStringList windowsModeNames() const;
    int downloadCount() const;
    QString licence() const { return m_licence; }
    QString licenceText() const { return m_licenceText; }

    Q_INVOKABLE void scan(const QString &windowsRoot, const QString &userHome);
    Q_INVOKABLE void setChecked(const QString &key, bool checked);
    /* Looks for a Windows licence: the key in the computer's firmware
     * (through the setup service, which alone can read it), else the
     * product key of the Windows on the drive. */
    Q_INVOKABLE void checkLicence();
    Q_INVOKABLE QVariantMap plan() const;
    /* Outside the setup session: starts the plan straight away. */
    Q_INVOKABLE void start() const;

    struct Found {
        QString name;
        QString publisher;
        QString folder;
        qint64 bytes = -1;
    };
    struct Scan {
        QList<Found> programs;
        bool registryRead = false;
        QString product;
        QString productKeyEnd;
    };
    static Scan read(const QString &windowsRoot, const QString &userHome);

Q_SIGNALS:
    void changed();
    void licenceChanged();

private:
    void classify(const Scan &scan);
    void setLicence(const QString &licence, const QString &text);

    QString m_state;
    QVariantList m_programs;
    bool m_registryRead = false;
    QString m_product;
    QString m_productKeyEnd;
    QString m_licence;
    QString m_licenceText;
    int m_generation = 0;
};
