#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>

// Step 5: finds Windows on this computer, reads the user folders on it and
// hands the chosen ones to the background copier. Windows drives are only
// ever mounted read-only.
class WindowsImport : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // scanning, found, none, locked (BitLocker) or error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(int current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(bool sizing READ sizing NOTIFY foldersChanged)
    Q_PROPERTY(qint64 selectedBytes READ selectedBytes NOTIFY foldersChanged)
    Q_PROPERTY(qint64 freeBytes READ freeBytes CONSTANT)
    Q_PROPERTY(bool started READ started NOTIFY startedChanged)
    // In the setup session the copy is planned, and runs once the new
    // account has signed in.
    Q_PROPERTY(bool planning READ planning CONSTANT)
    // The open Windows drive and the chosen user folder on it, for step 7.
    Q_PROPERTY(QString windowsRoot READ windowsRoot NOTIFY currentChanged)
    Q_PROPERTY(QString userHome READ userHome NOTIFY currentChanged)

public:
    explicit WindowsImport(QObject *parent = nullptr);
    ~WindowsImport() override;

    struct Drive {
        QString device;
        QString kname;
        QString label;
        QString mountPoint;
        bool weMounted = false;
        bool hasWindows = false;
        qint64 size = 0;
    };

    struct Source {
        int drive = -1;
        QString user;
        QString home;
    };

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QVariantList sources() const;
    int current() const { return m_current; }
    QVariantList folders() const { return m_folders; }
    bool sizing() const { return m_pendingSizes > 0; }
    qint64 selectedBytes() const;
    qint64 freeBytes() const;
    bool started() const { return m_started; }
    bool planning() const;
    QString windowsRoot() const;
    QString userHome() const;
    // Called from QML when setup finishes; without Q_INVOKABLE that call
    // throws and Finish does nothing.
    Q_INVOKABLE QVariantMap plan() const { return m_plan; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void choose(int index);
    Q_INVOKABLE void setChecked(const QString &key, bool checked);
    Q_INVOKABLE bool start();
    Q_INVOKABLE void release();
    Q_INVOKABLE QString formatBytes(qint64 bytes, bool whole = false) const;
    Q_INVOKABLE QString estimate() const;

Q_SIGNALS:
    void stateChanged();
    void sourcesChanged();
    void currentChanged();
    void foldersChanged();
    void startedChanged();

private:
    void setState(const QString &state, const QString &message = {});
    void readBlockDevices();
    void mountNext();
    void inspect(int driveIndex);
    void finishScan();
    void buildFolders();
    void unmount(const Drive &drive);
    QJsonObject job() const;

    QList<Drive> m_drives;
    QList<int> m_toMount;
    QList<Source> m_sources;
    QString m_state = QStringLiteral("scanning");
    QString m_message;
    int m_current = -1;
    QVariantList m_folders;
    int m_pendingSizes = 0;
    int m_generation = 0;
    bool m_started = false;
    bool m_sawBitLocker = false;
    QString m_mountError;
    QVariantMap m_plan;
};
