#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <functional>

#include <PackageKit/Transaction>

// Packages from the Wild OS repositories, through PackageKit. Installing,
// removing and updating go through polkit, so people get the usual password
// prompt; refreshing the catalogue is allowed without one (see the polkit
// rule in wildos-desktop).
class PackageKitBackend : public QObject
{
    Q_OBJECT

public:
    explicit PackageKitBackend(QObject *parent = nullptr);

    bool isAvailable() const;
    bool isReady() const { return m_ready; }

    void start();
    void refresh();
    void listInstalled();
    void listUpdates(std::function<void(bool ok)> done = {});

    void install(int job, const QStringList &packages);
    void remove(int job, const QStringList &packages);
    void update(int job, const QStringList &packages);
    void fetchDetails(const QString &key, const QStringList &packages);

    QSet<QString> installed() const { return m_installed; }
    QSet<QString> updates() const { return QSet<QString>(m_updateNames.begin(), m_updateNames.end()); }
    QString installedVersion(const QString &package) const { return m_versions.value(package); }

Q_SIGNALS:
    void installedChanged();
    void updatesChanged();
    // The catalogue on disk changed, so AppStream should be read again.
    void catalogueRefreshed();
    // A refresh asked for with refresh() is over, and the update list with it.
    void refreshFinished(bool ok);
    void progress(int job, int percent, const QString &status);
    void finished(int job, bool ok, const QString &error);
    void detailsReady(const QString &key, qint64 downloadSize, qint64 installedSize, const QString &version);

private:
    void resolveThen(const QStringList &packages, PackageKit::Transaction::Filters filters, int job,
                     std::function<PackageKit::Transaction *(const QStringList &ids)> next);
    void watch(int job, PackageKit::Transaction *transaction, bool removing);
    static QString errorText(PackageKit::Transaction::Error error, bool removing);

    QSet<QString> m_installed;
    QHash<QString, QString> m_versions;
    QHash<QString, QString> m_updateIds;
    QStringList m_updateNames;
    bool m_ready = false;
};
