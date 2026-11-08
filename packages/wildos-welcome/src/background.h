#pragma once

#include <KJob>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// The work that carries on after the welcome window has closed: copying
// files from Windows and installing the starter apps. Each runs as its own
// "wildos-welcome --copy" or "--install" process and shows its progress
// as a desktop job, the way copies in Files do.
namespace Background
{
int runCopy(const QString &jobFile);
int runInstall(const QString &jobFile);
void notify(const QString &summary, const QString &body, const QString &icon = QStringLiteral("wildos"));
}

class CopyJob : public KJob
{
    Q_OBJECT

public:
    explicit CopyJob(const QJsonObject &job, QObject *parent = nullptr);
    void start() override;

    qint64 copiedBytes() const { return m_copied; }
    int failures() const { return m_failures; }
    QStringList folderNames() const { return m_names; }
    int bookmarksAdded() const { return m_bookmarks; }
    int historyAdded() const { return m_history; }

protected:
    bool doKill() override;

private:
    void work();
    void importBookmarks();

    QJsonObject m_job;
    qint64 m_total = 0;
    qint64 m_copied = 0;
    int m_failures = 0;
    int m_bookmarks = -2;
    int m_history = -2;
    QStringList m_names;
    std::atomic_bool m_cancel = false;
};

class InstallJob : public KJob
{
    Q_OBJECT

public:
    explicit InstallJob(const QJsonArray &apps, QObject *parent = nullptr);
    void start() override;

    QStringList installed() const { return m_installed; }
    QStringList failed() const { return m_failed; }

private:
    void resolve();
    void installPackages(const QStringList &ids);
    void installFlatpaks();
    void done();

    QJsonArray m_apps;
    QHash<QString, QString> m_nameForPackage;
    QStringList m_flatpakQueue;
    QStringList m_installed;
    QStringList m_failed;
};
