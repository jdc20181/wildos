#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// The setup app's view of the PC and of the daemon (boards 9.1 to 9.7).
//
// The checks run here, against the real PC, as soon as the window opens.
// The licence is read through winmode-firmware (pkexec), because the MSDM
// table is root's to read. Setup itself runs in the daemon, so closing
// this window never stops it.
class SetupApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool checking READ checking NOTIFY checksChanged)
    // ok, off, unsupported or noaccess
    Q_PROPERTY(QString virtualisation READ virtualisation NOTIFY checksChanged)
    Q_PROPERTY(QString technology READ technology NOTIFY checksChanged)
    Q_PROPERTY(QString memoryLabel READ memoryLabel NOTIFY checksChanged)
    // ok, low (works, slowly) or short
    Q_PROPERTY(QString memoryState READ memoryState NOTIFY checksChanged)
    Q_PROPERTY(QString freeLabel READ freeLabel NOTIFY checksChanged)
    Q_PROPERTY(QString needLabel READ needLabel NOTIFY checksChanged)
    Q_PROPERTY(bool spaceOk READ spaceOk NOTIFY checksChanged)
    // checking, connected, metered or offline
    Q_PROPERTY(QString internet READ internet NOTIFY checksChanged)
    // checking, firmware, none, inuse (Windows is still on this PC) or error
    Q_PROPERTY(QString licence READ licence NOTIFY checksChanged)
    // home, pro or "" (not known until Windows installs)
    Q_PROPERTY(QString edition READ edition NOTIFY checksChanged)
    Q_PROPERTY(QString downloadsLabel READ downloadsLabel NOTIFY checksChanged)
    Q_PROPERTY(QString binLabel READ binLabel NOTIFY checksChanged)
    Q_PROPERTY(bool canRestartToFirmware READ canRestartToFirmware NOTIFY checksChanged)
    Q_PROPERTY(QVariantList memoryChoices READ memoryChoices NOTIFY checksChanged)

    Q_PROPERTY(bool office READ office WRITE setOffice NOTIFY optionsChanged)
    Q_PROPERTY(QString officePlan MEMBER m_officePlan NOTIFY optionsChanged)
    Q_PROPERTY(bool openFiles MEMBER m_openFiles NOTIFY optionsChanged)
    Q_PROPERTY(QString whenIdle MEMBER m_whenIdle NOTIFY optionsChanged)
    Q_PROPERTY(int memoryGb MEMBER m_memoryGb NOTIFY optionsChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY optionsChanged)

    // The daemon's Status, or empty while it has not answered.
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool preview READ preview CONSTANT)
    Q_PROPERTY(QString previewBoard READ previewBoard CONSTANT)

public:
    explicit SetupApp(const QString &preview = {}, QObject *parent = nullptr);

    bool checking() const { return m_pending > 0; }
    QString virtualisation() const { return m_virt; }
    QString technology() const { return m_technology; }
    QString memoryLabel() const { return m_memoryLabel; }
    QString memoryState() const { return m_memoryState; }
    QString freeLabel() const { return m_freeLabel; }
    QString needLabel() const;
    bool spaceOk() const;
    QString internet() const { return m_internet; }
    QString licence() const { return m_licence; }
    QString edition() const { return m_edition; }
    QString downloadsLabel() const { return m_downloadsLabel; }
    QString binLabel() const { return m_binLabel; }
    bool canRestartToFirmware() const { return m_canFirmware; }
    QVariantList memoryChoices() const;
    bool office() const { return m_office; }
    void setOffice(bool on);
    QStringList folders() const;
    QVariantMap status() const { return m_status; }
    bool preview() const { return !m_preview.isEmpty(); }
    QString previewBoard() const { return m_preview; }

    Q_INVOKABLE void recheck();
    Q_INVOKABLE void restartToFirmware();
    Q_INVOKABLE void openStorage();
    Q_INVOKABLE void openUrl(const QString &url);
    Q_INVOKABLE bool keyShapeValid(const QString &typed) const;
    Q_INVOKABLE QString normaliseKey(const QString &typed) const;
    // licence: firmware, key or none; homeChoice: upgrade or home
    Q_INVOKABLE QString startSetup(const QString &licence, const QString &key, const QString &homeChoice);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openDownloadPage();
    Q_INVOKABLE QString chooseIso(const QString &url);
    Q_INVOKABLE void openApp(const QString &id);
    // Whether the daemon has set up Windows mode already, or is doing so.
    Q_INVOKABLE QString daemonState() const;

Q_SIGNALS:
    void checksChanged();
    void optionsChanged();
    void statusChanged();

private Q_SLOTS:
    void refreshStatus();

private:
    void checkInternet();
    void checkLicence();
    void checkSpace();
    void finishOne();
    void loadPreview();

    QString m_preview;
    int m_pending = 0;
    QString m_virt = QStringLiteral("off");
    QString m_technology;
    qint64 m_memory = 0;
    QString m_memoryLabel;
    QString m_memoryState = QStringLiteral("ok");
    qint64 m_free = -1;
    QString m_freeLabel;
    QString m_internet = QStringLiteral("checking");
    QString m_licence = QStringLiteral("checking");
    QString m_edition;
    QString m_downloadsLabel;
    QString m_binLabel;
    bool m_canFirmware = false;

    bool m_office = true;
    QString m_officePlan = QStringLiteral("personal");
    bool m_openFiles = true;
    QString m_whenIdle = QStringLiteral("automatic");
    int m_memoryGb = 4;
    QStringList m_folders;

    QVariantMap m_status;
    QDBusInterface *m_daemon = nullptr;
};
