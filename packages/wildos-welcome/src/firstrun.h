#pragma once

#include <KJob>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

/* The account's side of first boot setup. Steps 4 to 7 are chosen in the
 * setup session, before this account has ever signed in, and saved as a
 * plan in its home (wildos-setup's Finish). At the first sign-in the
 * autostart entry runs "wildos-welcome --autostart", which finds the plan
 * and applies it here, as this account, with no window: the accent, colour
 * scheme and taskbar, the privacy choices, the copy from Windows, the
 * programs from step 7, and Windows mode's plan.
 */
namespace FirstRun
{
QString planFile();
bool hasPlan();
int apply();
// "wildos-welcome --programs <job>": fetches fresh Windows installers, then
// hands each to the App Bridge.
int runPrograms(const QString &jobFile);
// Step 7's choices: Store apps through "--install", installers through
// "--programs", each in a background process of its own.
void startPrograms(const QVariantList &programs);
// Windows mode itself is not built yet; what setup found is kept for it.
void saveWindowsMode(const QVariantMap &plan);
// Sends step 7's saved setup request (boards 3.9 and 3.10) to Windows
// mode's agent, as this account.
void startWindowsMode(const QString &request);
}

class DownloadJob : public KJob
{
    Q_OBJECT

public:
    explicit DownloadJob(const QJsonArray &programs, QObject *parent = nullptr);
    void start() override;

    QStringList ready() const { return m_ready; }
    QStringList failed() const { return m_failed; }

protected:
    bool doKill() override;

private:
    void next();
    void fetch(const QString &name, const QString &url, qint64 size, const QByteArray &sha256);
    void fail(const QString &name, const QString &why);

    QJsonArray m_programs;
    int m_index = -1;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QStringList m_ready;
    QStringList m_failed;
};
