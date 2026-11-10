#pragma once

#include <QDBusObjectPath>
#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include <PackageKit/Transaction>

// Checks for and installs system updates through PackageKit, the service the
// Store uses, so both always agree on what is waiting.
class Updates : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // idle, checking, available, uptodate, installing, installed, error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList updates READ updates NOTIFY updatesChanged)
    Q_PROPERTY(int securityCount READ securityCount NOTIFY updatesChanged)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY lastCheckedChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY restartNeededChanged)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit Updates(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QVariantList updates() const { return m_updates; }
    int securityCount() const;
    QDateTime lastChecked() const { return m_lastChecked; }
    QVariantList history() const { return m_history; }
    int progress() const { return m_progress; }
    QString detail() const { return m_detail; }
    QString error() const { return m_error; }
    bool restartNeeded() const { return m_restartNeeded; }
    bool available() const;

    Q_INVOKABLE void check();
    Q_INVOKABLE void listUpdates();
    Q_INVOKABLE void install();
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void restart();
    // What a package is to a person: {group, title}. group is one of
    // wildos, core, browser, desktop, app, library or other; title is the
    // app's own name for an app and empty otherwise.
    Q_INVOKABLE QVariantMap describe(const QString &package) const;
    // The newest entry of a Wild OS package's changelog, for "What's new".
    Q_INVOKABLE QString releaseNotes() const;

Q_SIGNALS:
    void stateChanged();
    void updatesChanged();
    void lastCheckedChanged();
    void historyChanged();
    void progressChanged();
    void restartNeededChanged();

private Q_SLOTS:
    void onOldTransaction(const QDBusObjectPath &tid, const QString &timespec, bool succeeded, uint role, uint duration, const QString &data,
                          uint uid, const QString &cmdline);
    void onHistoryFinished(uint exit, uint runtime);

private:
    void setState(const QString &state, const QString &error = {});
    void readLastChecked();
    void watchProgress(PackageKit::Transaction *transaction);
    static QString errorText(PackageKit::Transaction::Error error, bool installing);
    void startInstall();
    void refreshThenInstall();

    QString m_state = QStringLiteral("idle");
    QVariantList m_updates;
    QStringList m_pending;
    QDateTime m_lastChecked;
    QVariantList m_history;
    QVariantList m_collected;
    QString m_historyPath;
    int m_progress = 0;
    QString m_detail;
    QString m_error;
    bool m_restartNeeded = false;
    // An install that failed because the update lists were out of date
    // (a package file the lists name is gone) checks again and retries
    // once by itself.
    bool m_staleRetrying = false;
    bool m_staleRetried = false;
    bool m_installAfterList = false;
};
