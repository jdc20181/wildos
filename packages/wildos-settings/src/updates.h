#pragma once

#include <QDBusObjectPath>
#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include <PackageKit/Transaction>

#include <functional>

// Checks for and installs system updates through PackageKit, the service the
// Store uses, so both always agree on what is waiting.
class QNetworkAccessManager;

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
    // Another install (usually the automatic security updates) holds the
    // package system, and the check or install waits for it.
    Q_PROPERTY(bool waiting READ waiting NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    // Software sources added by other software that the last check could
    // not use, in a sentence, while everything else was checked. Empty when
    // none failed.
    Q_PROPERTY(QString skippedSources READ skippedSources NOTIFY stateChanged)
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY restartNeededChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    // Wild OS builds: the one installed and the one waiting, 0 when unknown
    // or nothing from Wild OS is waiting. A build is the +rN every Wild OS
    // package's version carries.
    Q_PROPERTY(int installedBuild READ installedBuild NOTIFY installedBuildChanged)
    Q_PROPERTY(int pendingBuild READ pendingBuild NOTIFY updatesChanged)
    // What each recent build changed, newest first, from notes.json on the
    // update site: {build, date, items: [{category, text}]}, category new,
    // improved or fixed. Empty when it could never be fetched.
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)

public:
    explicit Updates(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QVariantList updates() const { return m_updates; }
    int securityCount() const;
    QDateTime lastChecked() const { return m_lastChecked; }
    QVariantList history() const { return m_history; }
    int progress() const { return m_progress; }
    QString detail() const { return m_detail; }
    bool waiting() const { return m_waiting; }
    QString error() const { return m_error; }
    QString skippedSources() const { return m_skipped; }
    bool restartNeeded() const { return m_restartNeeded; }
    bool available() const;
    int installedBuild() const { return m_installedBuild; }
    int pendingBuild() const { return m_pendingBuild; }
    QVariantList notes() const { return m_notes; }

    Q_INVOKABLE void check();
    Q_INVOKABLE void listUpdates();
    Q_INVOKABLE void install();
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void restart();
    // What a package is to a person: {group, title}. group is one of
    // wildos, core, browser, desktop, app, library or other; title is the
    // app's own name for an app and empty otherwise.
    Q_INVOKABLE QVariantMap describe(const QString &package) const;
    // The notes of the builds after `after`, up to and including `upTo`.
    Q_INVOKABLE QVariantList notesBetween(int after, int upTo) const;
    // Those builds' changes in a few words: "3 fixes and 2 improvements".
    Q_INVOKABLE QString summary(int after, int upTo) const;
    Q_INVOKABLE void fetchNotes();

Q_SIGNALS:
    void stateChanged();
    void updatesChanged();
    void lastCheckedChanged();
    void historyChanged();
    void progressChanged();
    void restartNeededChanged();
    void installedBuildChanged();
    void notesChanged();

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
    void refreshForCheck();
    bool onlyOthersFailed(const QString &details);
    void readUpdates(bool quiet);
    bool lockHeld(PackageKit::Transaction::Error error);
    void retryAfterLock(const std::function<void()> &retry);
    void setWaiting(bool waiting);
    void readInstalledBuild();
    bool readNotes(const QByteArray &json);

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
    QString m_skipped;
    bool m_partialRefresh = false;
    bool m_restartNeeded = false;
    // An install that failed because the update lists were out of date
    // (a package file the lists name is gone) checks again and retries
    // once by itself.
    bool m_staleRetrying = false;
    bool m_staleRetried = false;
    bool m_installAfterList = false;
    // Set by a transaction that failed only because the lock was held; it
    // runs again once the wait below allows.
    bool m_lockBlocked = false;
    QElapsedTimer m_lockWait;
    bool m_waiting = false;
    int m_installedBuild = 0;
    int m_pendingBuild = 0;
    QVariantList m_notes;
    QNetworkAccessManager *m_network = nullptr;
};
