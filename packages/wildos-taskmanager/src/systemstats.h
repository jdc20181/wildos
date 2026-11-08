#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

namespace KSysGuard
{
class Sensor;
}

// The Performance page's numbers: one resource per sidebar tile (the
// processor, memory, the Wild OS disk and the Windows C: disk, the network
// connection, graphics and Windows mode), each with a 60 second history
// and the live stats and specs its page shows.
//
// Live values come from ksystemstats, the desktop's system monitor service,
// through libksysguard's Sensor. Where it has no sensor for something the
// page needs (a disk's active time, a partition that is not mounted, the
// per-process graphics use behind "Using the most"), the kernel's own
// files are read: /proc/stat, /proc/meminfo, /sys/class/block, DRM fdinfo.
// Hardware facts (cores, caches, base speed, disk kind) come from sysfs.
class SystemStats : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList resources READ resources NOTIFY updated)
    Q_PROPERTY(QStringList resourceIds READ resourceIds NOTIFY resourceIdsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY updated)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY updated)
    Q_PROPERTY(double memoryPercent READ memoryPercent NOTIFY updated)
    Q_PROPERTY(double diskPercent READ diskPercent NOTIFY updated)
    Q_PROPERTY(double networkPercent READ networkPercent NOTIFY updated)
    Q_PROPERTY(qint64 memoryTotal READ memoryTotal NOTIFY updated)
    Q_PROPERTY(qint64 memoryUsed READ memoryUsed NOTIFY updated)
    Q_PROPERTY(int coreCount READ coreCount NOTIFY updated)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
    static SystemStats *instance();
    static SystemStats *create(QQmlEngine *, QJSEngine *);

    QVariantList resources() const;
    QStringList resourceIds() const { return m_resourceIds; }
    Q_INVOKABLE QVariantMap resource(const QString &id) const;
    int revision() const { return m_revision; }
    double cpuUsage() const { return m_cpu; }
    double memoryPercent() const { return m_memTotal > 0 ? 100.0 * m_memUsed / m_memTotal : 0; }
    double diskPercent() const;
    double networkPercent() const;
    qint64 memoryTotal() const { return m_memTotal; }
    qint64 memoryUsed() const { return m_memUsed; }
    int coreCount() const { return m_cores.size(); }
    int interval() const { return m_timer.interval(); }
    void setInterval(int ms);
    bool paused() const { return m_paused; }
    void setPaused(bool paused);

    // Everything one resource's page shows: title, subtitle, the graph's
    // caption and scale, history, stats, specs and the "Using the most"
    // list's title and metric.
    Q_INVOKABLE QVariantMap details(const QString &id) const;
    // One history per logical processor, oldest first.
    Q_INVOKABLE QVariantList coreHistories() const;

Q_SIGNALS:
    void updated();
    void resourceIdsChanged();
    void intervalChanged();
    void pausedChanged();

private:
    struct Disk {
        QString id;
        QString title;
        QString device;
        QString parent;
        QString uuid;
        QString fsType;
        QString mount;
        QString model;
        bool ssd = true;
        bool system = false;
        qint64 size = 0;
        qint64 used = -1;
        qint64 free = -1;
        quint64 ticks = 0;
        quint64 readSectors = 0;
        quint64 writeSectors = 0;
        double active = 0;
        double readRate = 0;
        double writeRate = 0;
        bool sampled = false;
        QList<double> history;
    };

    explicit SystemStats(QObject *parent = nullptr);
    void sample();
    void sampleCpu(double seconds);
    void sampleMemory();
    void sampleDisks(double seconds);
    void sampleNetwork(double seconds);
    void sampleGpu(double seconds);
    void findDisks();
    void findNetwork();
    void readHardware();
    void push(QList<double> &history, double value) const;
    KSysGuard::Sensor *sensor(const QString &id);
    QVariant sensorValue(const QString &id) const;
    qint64 linkSpeed() const;

    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_paused = false;
    int m_revision = 0;
    QStringList m_resourceIds;
    QHash<QString, KSysGuard::Sensor *> m_sensors;

    // Processor
    double m_cpu = 0;
    QList<double> m_cpuHistory;
    QList<QList<double>> m_cores;
    QList<QPair<quint64, quint64>> m_cpuTicks;
    double m_speedMhz = 0;
    double m_temperature = 0;
    QString m_cpuModel;
    QString m_baseSpeed;
    QString m_coreText;
    QString m_virtualisation;
    QString m_cache;

    // Memory
    qint64 m_memTotal = 0;
    qint64 m_memUsed = 0;
    qint64 m_memAvailable = 0;
    qint64 m_memCached = 0;
    qint64 m_memFree = 0;
    qint64 m_swapTotal = 0;
    qint64 m_swapUsed = 0;
    QString m_swapKind;
    QList<double> m_memHistory;

    QList<Disk> m_disks;

    // Network
    QString m_iface;
    bool m_wireless = false;
    quint64 m_rxBytes = 0;
    quint64 m_txBytes = 0;
    double m_rx = 0;
    double m_tx = 0;
    double m_netMax = 0;
    qint64 m_link = 0;
    int m_linkTick = 0;
    QList<double> m_rxHistory;
    QList<double> m_txHistory;

    // Graphics
    bool m_hasGpu = false;
    QString m_gpuName;
    QString m_gpuDriver;
    QString m_gpuCard;
    double m_gpu = 0;
    QList<double> m_gpuHistory;
    QHash<QString, QHash<QString, quint64>> m_gpuClients;
    int m_gpuTick = 0;

    // Windows mode
    QList<double> m_winHistory;
    int m_winInstalled = 0;
};
