#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTime>
#include <QVariantList>

class QSettings;

// What the person chose: their drives, what goes in, what is kept and when
// it runs. Kept in ~/.config/wildos/backup.conf, which the scheduled run
// reads too.
class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ uuid, name, kind, isDefault }]
    Q_PROPERTY(QVariantList drives READ drives NOTIFY drivesChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY drivesChanged)
    Q_PROPERTY(QString defaultDrive READ defaultDrive NOTIFY drivesChanged)

    Q_PROPERTY(bool includeHome READ includeHome WRITE setIncludeHome NOTIFY includeChanged)
    Q_PROPERTY(bool includeMail READ includeMail WRITE setIncludeMail NOTIFY includeChanged)
    Q_PROPERTY(bool includeSettings READ includeSettings WRITE setIncludeSettings NOTIFY includeChanged)
    Q_PROPERTY(bool includeWindows READ includeWindows WRITE setIncludeWindows NOTIFY includeChanged)
    Q_PROPERTY(QStringList excludes READ excludes NOTIFY includeChanged)

    Q_PROPERTY(QString keepWithin READ keepWithin WRITE setKeepWithin NOTIFY keepChanged)
    Q_PROPERTY(int keepDaily READ keepDaily WRITE setKeepDaily NOTIFY keepChanged)
    Q_PROPERTY(int keepWeekly READ keepWeekly WRITE setKeepWeekly NOTIFY keepChanged)
    Q_PROPERTY(int keepMonthly READ keepMonthly WRITE setKeepMonthly NOTIFY keepChanged)

    Q_PROPERTY(bool scheduled READ scheduled WRITE setScheduled NOTIFY scheduleChanged)
    // Minutes after midnight.
    Q_PROPERTY(int time READ time WRITE setTime NOTIFY scheduleChanged)
    // 1 Monday to 7 Sunday, as Qt numbers them.
    Q_PROPERTY(QVariantList days READ days WRITE setDays NOTIFY scheduleChanged)

    Q_PROPERTY(bool notifyFinished READ notifyFinished WRITE setNotifyFinished NOTIFY notifyChanged)

public:
    // No default: QML would make a second one instead of calling create().
    explicit Settings(QObject *parent);
    ~Settings() override;
    static Settings *instance();
    static Settings *create(QQmlEngine *, QJSEngine *);

    QVariantList drives() const;
    QJsonArray driveList() const;
    bool configured() const;
    QString defaultDrive() const;
    QJsonObject drive(const QString &uuid) const;
    void addDrive(const QString &uuid, const QString &name, const QString &kind);
    Q_INVOKABLE void removeDrive(const QString &uuid);
    Q_INVOKABLE void makeDefault(const QString &uuid);
    // What the scheduled run notes per drive: lastBackup, lastPrune.
    void setDriveValue(const QString &uuid, const QString &key, const QJsonValue &value);

    bool includeHome() const;
    bool includeMail() const;
    bool includeSettings() const;
    bool includeWindows() const;
    void setIncludeHome(bool on);
    void setIncludeMail(bool on);
    void setIncludeSettings(bool on);
    void setIncludeWindows(bool on);
    QStringList excludes() const;
    Q_INVOKABLE void addExclude(const QString &path);
    Q_INVOKABLE void removeExclude(const QString &path);

    QString keepWithin() const;
    int keepDaily() const;
    int keepWeekly() const;
    int keepMonthly() const;
    void setKeepWithin(const QString &value);
    void setKeepDaily(int value);
    void setKeepWeekly(int value);
    void setKeepMonthly(int value);
    // The restic forget options for the Keep choices.
    QStringList keepArguments() const;

    bool scheduled() const;
    int time() const;
    QVariantList days() const;
    QList<int> dayList() const;
    void setScheduled(bool on);
    void setTime(int minutes);
    void setDays(const QVariantList &days);
    // The next scheduled run after `from`, or an invalid time if none.
    QDateTime nextRun(const QDateTime &from) const;
    // The latest scheduled run at or before `from`.
    QDateTime previousRun(const QDateTime &from) const;

    // The new password was shown and its dialog closed. Until then, every
    // new repository shows it again, even made with a password saved by an
    // earlier setup that didn't finish.
    bool passwordShown() const;
    void setPasswordShown(bool shown);

    bool notifyFinished() const;
    void setNotifyFinished(bool on);

    // Another process may have changed the drive list or its notes.
    void reload();

Q_SIGNALS:
    void drivesChanged();
    void includeChanged();
    void keepChanged();
    void scheduleChanged();
    void notifyChanged();

private:
    void setDrives(const QJsonArray &drives);
    void setValue(const QString &key, const QVariant &value);

    QSettings *m_settings;
};
