#include "setupapp.h"
#include "licence.h"
#include "options.h"
#include "pccheck.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace
{
// The per-user agent of Windows mode (wildos-winmode's agent.py), which
// passes setup on to the system service.
const QString agentName = QStringLiteral("org.wildos.WindowsMode1");
const QString agentPath = QStringLiteral("/org/wildos/WindowsMode1");
}

SetupApp::SetupApp(const QString &preview, QObject *parent)
    : QObject(parent)
    , m_preview(preview)
{
    const Options saved = Options::load();
    m_office = saved.office;
    m_officePlan = saved.officePlan;
    m_openFiles = saved.openFiles;
    m_whenIdle = saved.whenIdle;
    m_memoryGb = saved.memoryGb;
    m_folders = saved.folders;

    if (preview.isEmpty()) {
        m_daemon = new QDBusInterface(agentName, agentPath, agentName, QDBusConnection::sessionBus(), this);
        QDBusConnection::sessionBus().connect(agentName, agentPath,
                                              QStringLiteral("org.wildos.WindowsMode1"), QStringLiteral("Changed"), this, SLOT(refreshStatus()));
        refreshStatus();
        recheck();
    } else {
        loadPreview();
    }
}

void SetupApp::loadPreview()
{
    // Made-up values for design reviews (--preview), as on the boards.
    m_virt = QStringLiteral("ok");
    m_technology = QStringLiteral("Intel VT-x");
    m_memory = qint64(16) * 1024 * 1024 * 1024;
    m_memoryLabel = QStringLiteral("16 GB");
    m_free = qint64(412) * 1000 * 1000 * 1000;
    m_freeLabel = QStringLiteral("412 GB");
    m_internet = QStringLiteral("connected");
    m_licence = m_preview == QLatin1String("nolicence") ? QStringLiteral("none") : QStringLiteral("firmware");
    m_edition = m_preview == QLatin1String("home") ? QStringLiteral("home") : QStringLiteral("pro");
    m_downloadsLabel = QStringLiteral("9.4 GB");
    m_binLabel = QStringLiteral("3.1 GB");
    m_canFirmware = true;
    if (m_preview == QLatin1String("problems")) {
        m_virt = QStringLiteral("off");
        m_free = qint64(18) * 1000 * 1000 * 1000;
        m_freeLabel = QStringLiteral("18 GB");
    }
    const auto stage = [](const QString &id, const QString &title, const QString &detail, const QString &state, int percent) {
        return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), title}, {QStringLiteral("detail"), detail},
                           {QStringLiteral("state"), state}, {QStringLiteral("percent"), percent}};
    };
    if (m_preview == QLatin1String("setup") || m_preview == QLatin1String("download")) {
        const bool paused = m_preview == QLatin1String("download");
        m_status = QVariantMap{
            {QStringLiteral("state"), QStringLiteral("setup")},
            {QStringLiteral("activeStage"), paused ? QStringLiteral("media") : QStringLiteral("install")},
            {QStringLiteral("percent"), paused ? 15 : 46},
            {QStringLiteral("left"), QStringLiteral("About 14 minutes left")},
            {QStringLiteral("paused"), paused},
            {QStringLiteral("problem"), paused ? QVariantMap{{QStringLiteral("kind"), QStringLiteral("download")}, {QStringLiteral("received"), 3.8e9}} : QVariantMap()},
            {QStringLiteral("stages"), QVariantList{
                 stage(QStringLiteral("media"), QStringLiteral("Download Windows 11 from Microsoft"), QStringLiteral("6.1 GB, checked against Microsoft's signature"),
                       paused ? QStringLiteral("active") : QStringLiteral("done"), paused ? 62 : 100),
                 stage(QStringLiteral("install"), QStringLiteral("Install Windows"), QStringLiteral("Automatic, no questions to answer"),
                       paused ? QStringLiteral("pending") : QStringLiteral("active"), 61),
                 stage(QStringLiteral("activate"), QStringLiteral("Activate with this PC's licence"), QStringLiteral("Windows 11 Pro"), QStringLiteral("pending"), 0),
                 stage(QStringLiteral("tune"), QStringLiteral("Tune for speed and privacy"), QStringLiteral("Removes adverts, tracking and apps you won't use"),
                       QStringLiteral("pending"), 0),
                 stage(QStringLiteral("office"), QStringLiteral("Install Microsoft 365"), QStringLiteral("Word, Excel, PowerPoint, Outlook, OneNote"),
                       QStringLiteral("pending"), 0),
             }},
        };
    } else if (m_preview == QLatin1String("ready")) {
        const auto app = [](const QString &id, const QString &name) {
            return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("icon"), QStringLiteral("wildos-winmode-") + id}};
        };
        m_status = QVariantMap{
            {QStringLiteral("state"), QStringLiteral("ready")},
            {QStringLiteral("vm"), QStringLiteral("paused")},
            {QStringLiteral("licence"), QVariantMap{{QStringLiteral("source"), QStringLiteral("firmware")}, {QStringLiteral("edition"), QStringLiteral("pro")},
                                                    {QStringLiteral("activated"), true}}},
            {QStringLiteral("options"), QVariantMap{{QStringLiteral("office"), true}, {QStringLiteral("whenIdle"), QStringLiteral("automatic")}}},
            {QStringLiteral("apps"), QVariantList{app(QStringLiteral("word"), QStringLiteral("Word")), app(QStringLiteral("excel"), QStringLiteral("Excel")),
                                                  app(QStringLiteral("powerpoint"), QStringLiteral("PowerPoint")), app(QStringLiteral("outlook"), QStringLiteral("Outlook")),
                                                  app(QStringLiteral("onenote"), QStringLiteral("OneNote"))}},
        };
    }
}

void SetupApp::refreshStatus()
{
    if (!m_daemon) {
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(m_daemon->asyncCall(QStringLiteral("Status")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QString> reply = *watcher;
        m_status = reply.isError() ? QVariantMap() : QJsonDocument::fromJson(reply.value().toUtf8()).object().toVariantMap();
        Q_EMIT statusChanged();
    });
}

QString SetupApp::daemonState() const
{
    return m_status.value(QStringLiteral("state")).toString();
}

void SetupApp::finishOne()
{
    if (m_pending > 0) {
        --m_pending;
    }
    Q_EMIT checksChanged();
}

void SetupApp::recheck()
{
    if (preview()) {
        return;
    }
    m_pending = 3;
    const PcCheck::Virtualisation v = PcCheck::virtualisation();
    m_virt = v.state;
    m_technology = v.technology;
    m_memory = PcCheck::memoryBytes();
    m_memoryLabel = PcCheck::memoryLabel(m_memory);
    const double gb = double(m_memory) / (1024.0 * 1024 * 1024);
    m_memoryState = gb >= 7.2 ? QStringLiteral("ok") : gb >= 5.4 ? QStringLiteral("low") : QStringLiteral("short");
    if (m_memoryGb * 1024.0 * 1024 * 1024 > m_memory * 0.75) {
        m_memoryGb = 4;
    }
    QDBusInterface login(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"),
                         QDBusConnection::systemBus());
    const QDBusReply<QString> can = login.call(QStringLiteral("CanRebootToFirmwareSetup"));
    m_canFirmware = can.isValid() && (can.value() == QLatin1String("yes") || can.value() == QLatin1String("challenge"));
    Q_EMIT checksChanged();
    checkSpace();
    checkInternet();
    checkLicence();
}

void SetupApp::checkSpace()
{
    // Windows mode keeps Windows in /var/lib/wildos/winmode, which may not
    // exist yet.
    m_free = PcCheck::freeBytes(QStringLiteral("/var/lib"));
    m_freeLabel = m_free >= 0 ? PcCheck::sizeLabel(m_free) : QString();
    // Sizing two folders can take a moment on a full disk; off the main
    // thread so the window stays responsive.
    auto *thread = QThread::create([this] {
        const qint64 downloads = PcCheck::folderBytes(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        const qint64 bin = PcCheck::folderBytes(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files"));
        QMetaObject::invokeMethod(this, [this, downloads, bin] {
            m_downloadsLabel = PcCheck::sizeLabel(downloads);
            m_binLabel = PcCheck::sizeLabel(bin);
            finishOne();
        });
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(QThread::LowPriority);
}

void SetupApp::checkInternet()
{
    m_internet = QStringLiteral("checking");
    QDBusInterface nm(QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/org/freedesktop/NetworkManager"),
                      QStringLiteral("org.freedesktop.NetworkManager"), QDBusConnection::systemBus());
    if (nm.isValid()) {
        const uint connectivity = nm.property("Connectivity").toUInt();
        const uint metered = nm.property("Metered").toUInt();
        // 4 is full internet; 1 and 3 are metered, yes and guessed yes.
        m_internet = connectivity == 4 ? ((metered == 1 || metered == 3) ? QStringLiteral("metered") : QStringLiteral("connected")) : QStringLiteral("offline");
        finishOne();
        return;
    }
    // No NetworkManager to ask: try Microsoft itself.
    auto *socket = new QTcpSocket(this);
    auto *timer = new QTimer(socket);
    timer->setSingleShot(true);
    connect(socket, &QTcpSocket::connected, this, [this, socket] {
        if (socket->property("done").toBool()) {
            return;
        }
        socket->setProperty("done", true);
        m_internet = QStringLiteral("connected");
        socket->deleteLater();
        finishOne();
    });
    const auto offline = [this, socket] {
        if (socket->property("done").toBool()) {
            return;
        }
        socket->setProperty("done", true);
        m_internet = QStringLiteral("offline");
        socket->deleteLater();
        finishOne();
    };
    connect(socket, &QTcpSocket::errorOccurred, this, offline);
    connect(timer, &QTimer::timeout, this, offline);
    timer->start(4000);
    socket->connectToHost(QStringLiteral("www.microsoft.com"), 443);
}

void SetupApp::checkLicence()
{
    m_licence = QStringLiteral("checking");
    // For the rig and the tests: the answer from a file.
    const QString canned = qEnvironmentVariable("WILDOS_WINMODE_FIRMWARE");
    const auto apply = [this](const QByteArray &json, bool ok) {
        const QJsonObject fw = QJsonDocument::fromJson(json).object();
        if (!ok || fw.isEmpty() || fw.contains(QStringLiteral("error"))) {
            m_licence = QStringLiteral("error");
        } else {
            m_edition = Licence::editionFrom(fw.value(QStringLiteral("edition")).toString(), fw.value(QStringLiteral("description")).toString());
            // One licence covers one copy of Windows; with Windows still on
            // this PC the built-in one is taken.
            m_licence = !fw.value(QStringLiteral("msdm")).toBool() ? QStringLiteral("none")
                : fw.value(QStringLiteral("windows")).toBool()     ? QStringLiteral("inuse")
                                                                   : QStringLiteral("firmware");
        }
        finishOne();
    };
    if (!canned.isEmpty()) {
        QFile file(canned);
        apply(file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(), file.isOpen());
        return;
    }
    // The system service reads the firmware's licence table and looks for
    // Windows on the disks, which can take a little while.
    m_daemon->setTimeout(120000);
    auto *watcher = new QDBusPendingCallWatcher(m_daemon->asyncCall(QStringLiteral("Firmware")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, apply] {
        watcher->deleteLater();
        const QDBusPendingReply<QString> reply = *watcher;
        apply(reply.isError() ? QByteArray() : reply.value().toUtf8(), !reply.isError());
    });
}

QString SetupApp::needLabel() const
{
    return PcCheck::sizeLabel(PcCheck::neededBytes(m_office));
}

bool SetupApp::spaceOk() const
{
    return m_free < 0 || m_free >= PcCheck::neededBytes(m_office);
}

QVariantList SetupApp::memoryChoices() const
{
    QVariantList choices;
    for (int gb : {4, 6, 8, 12, 16, 24, 32}) {
        if (gb == 4 || gb * 1024.0 * 1024 * 1024 <= m_memory * 0.75) {
            choices << QVariantMap{{QStringLiteral("value"), gb}, {QStringLiteral("text"), QStringLiteral("Up to %1 GB").arg(gb)}};
        }
    }
    return choices;
}

void SetupApp::setOffice(bool on)
{
    if (m_office != on) {
        m_office = on;
        Q_EMIT optionsChanged();
        Q_EMIT checksChanged();
    }
}

QStringList SetupApp::folders() const
{
    return m_folders;
}

void SetupApp::restartToFirmware()
{
    QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("reboot"), QStringLiteral("--firmware-setup")});
}

void SetupApp::openStorage()
{
    QProcess::startDetached(QStringLiteral("wildos-settings"), {QStringLiteral("--page"), QStringLiteral("storage")});
}

void SetupApp::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

bool SetupApp::keyShapeValid(const QString &typed) const
{
    return Licence::keyShapeValid(Licence::normaliseKey(typed));
}

QString SetupApp::normaliseKey(const QString &typed) const
{
    return Licence::normaliseKey(typed);
}

namespace
{
const QString cpuThreadsHelper = QStringLiteral("/usr/libexec/wildos/settings-cpu-threads");

QByteArray readSmt(const char *name)
{
    QFile file(QStringLiteral("/sys/devices/system/cpu/smt/") + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
}
}

// Offered only on a processor with two threads per core, while it is off, and
// not at Strict or Maximum protection, which keep it off.
bool SetupApp::hyperThreadingAvailable() const
{
    const QByteArray control = readSmt("control");
    QFile levelFile(QStringLiteral("/etc/wildos/protection-level"));
    const QByteArray level = levelFile.open(QIODevice::ReadOnly) ? levelFile.readAll().trimmed() : QByteArray();
    return QFile::exists(cpuThreadsHelper) && !control.isEmpty() && control != "notsupported" && control != "notimplemented"
        && !QFile::exists(QStringLiteral("/etc/wildos/cpu-threads")) && level != "strict" && level != "maximum";
}

bool SetupApp::hyperThreadingNeedsRestart() const
{
    return QFile::exists(QStringLiteral("/etc/wildos/cpu-threads")) && readSmt("active") != "1";
}

QString SetupApp::startSetup(const QString &licence, const QString &key, const QString &homeChoice)
{
    if (preview()) {
        return {};
    }
    // A boot option, so it needs the administrator; setup goes ahead either way.
    if (m_hyperThreading && hyperThreadingAvailable()) {
        auto process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            Q_EMIT optionsChanged();
        });
        process->start(QStringLiteral("pkexec"), {cpuThreadsHelper, QStringLiteral("on")});
    }
    Options o = Options::load();
    o.office = m_office;
    o.officePlan = m_officePlan;
    o.openFiles = m_openFiles;
    o.whenIdle = m_whenIdle;
    o.memoryGb = m_memoryGb;
    // The firmware's key stays with the system service, which reads it
    // itself; only a key typed here travels with the request.
    QJsonObject lic{{QStringLiteral("source"), licence}, {QStringLiteral("edition"), m_edition}};
    if (licence == QLatin1String("key")) {
        lic.insert(QStringLiteral("key"), Licence::normaliseKey(key));
        lic.insert(QStringLiteral("edition"), QString());
    } else {
        lic.insert(QStringLiteral("edition"), m_edition);
    }
    const QJsonObject request{{QStringLiteral("licence"), lic}, {QStringLiteral("homeChoice"), homeChoice}, {QStringLiteral("options"), o.toJson()}};
    m_daemon->setTimeout(120000);
    const QDBusReply<QString> reply = m_daemon->call(QStringLiteral("StartSetup"), QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    if (!reply.isValid()) {
        return QStringLiteral("Windows mode's background service didn't start. Restart the PC, then try again.");
    }
    refreshStatus();
    return reply.value();
}

void SetupApp::pause()
{
    if (m_daemon) {
        m_daemon->asyncCall(QStringLiteral("PauseSetup"));
    }
}

void SetupApp::resume()
{
    if (m_daemon) {
        m_daemon->asyncCall(QStringLiteral("ResumeSetup"));
    }
}

void SetupApp::cancel()
{
    if (m_daemon) {
        m_daemon->asyncCall(QStringLiteral("CancelSetup"));
    }
}

void SetupApp::openDownloadPage()
{
    if (m_daemon) {
        m_daemon->asyncCall(QStringLiteral("OpenDownloadPage"));
    }
}

QString SetupApp::chooseIso(const QString &url)
{
    if (!m_daemon) {
        return {};
    }
    const QDBusReply<QString> reply = m_daemon->call(QStringLiteral("ChooseIso"), QUrl(url).toLocalFile());
    return reply.isValid() ? reply.value() : QStringLiteral("Windows mode's background service didn't answer.");
}

void SetupApp::openApp(const QString &id)
{
    if (preview()) {
        return;
    }
    QProcess::startDetached(QStringLiteral("wildos-winmode"), {QStringLiteral("launch"), id});
}
