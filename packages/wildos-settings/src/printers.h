#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include <atomic>
#include <functional>
#include <memory>

// Printers through CUPS: the queues on this computer with their state, the
// printers CUPS finds on the network (DNS-SD, the way IPP Everywhere
// printers announce themselves), adding one driverless with lpadmin's
// "everywhere" model, the account's default printer (lpoptions, as Windows
// keeps the default per person), a test page, and removing a queue.
//
// Adding and removing are CUPS administration. Members of lpadmin (every
// administrator Wild OS creates) are let in by CUPS itself over the local
// socket; anyone else is asked for an administrator's password through
// pkexec.
class Printers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList printers READ printers NOTIFY changed)
    Q_PROPERTY(QVariantList found READ found NOTIFY foundChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY foundChanged)
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)

public:
    explicit Printers(QObject *parent = nullptr);
    ~Printers() override;

    bool available() const { return m_available; }
    QVariantList printers() const { return m_printers; }
    QVariantList found() const { return m_found; }
    bool searching() const { return m_searching; }
    QString busy() const { return m_busy; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void search();
    Q_INVOKABLE void stopSearch();
    // uri is a device URI from `found` (dnssd://...) or ipp://host/...
    Q_INVOKABLE void add(const QString &uri, const QString &name);
    // A printer by its network name or IP address, for one that does not
    // announce itself.
    Q_INVOKABLE void addAddress(const QString &address);
    Q_INVOKABLE void setDefault(const QString &name, bool announce = true);
    Q_INVOKABLE void printTestPage(const QString &name);
    Q_INVOKABLE void remove(const QString &name);
    Q_INVOKABLE void resume(const QString &name);
    Q_INVOKABLE void cancelJobs(const QString &name);

Q_SIGNALS:
    void changed();
    void foundChanged();
    void busyChanged();
    void failed(const QString &title, const QString &reason);
    void done(const QString &title, const QString &message);

private:
    void setBusy(const QString &busy);
    void admin(const QString &busy, const QString &program, const QStringList &arguments, const QString &successTitle, const QString &success,
               const QString &failure, const std::function<void()> &then = {});
    QString queueName(const QString &name) const;

    bool m_available = false;
    QVariantList m_printers;
    QVariantList m_found;
    bool m_searching = false;
    QString m_busy;
    std::shared_ptr<std::atomic<int>> m_cancelSearch;
    int m_searchGeneration = 0;
};
