#include "updates.h"

#include <KService>

#include <PackageKit/Daemon>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

using PackageKit::Daemon;
using PackageKit::Transaction;

namespace
{
// Where Settings remembers, until the PC restarts, that an update asked for
// a restart: the runtime folder is emptied at every boot.
QString restartMarker()
{
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/wildos-restart-needed");
}

// Debian rarely says a restart is needed (no /run/reboot-required without
// update-notifier, and PackageKit only for a few packages), so Wild OS
// decides from what was installed: anything that only takes effect at
// start-up or sign-in. Apps pick their update up the next time they open.
bool needsRestart(const QStringList &packageIds)
{
    static const QStringList prefixes = {
        QStringLiteral("linux-image"), QStringLiteral("firmware-"), QStringLiteral("amd64-microcode"), QStringLiteral("intel-microcode"),
        QStringLiteral("systemd"), QStringLiteral("libc6"), QStringLiteral("dbus"), QStringLiteral("dracut"), QStringLiteral("grub"),
        QStringLiteral("plymouth"), QStringLiteral("sddm"), QStringLiteral("kwin"), QStringLiteral("plasma-workspace"),
        QStringLiteral("plasma-desktop"), QStringLiteral("libplasma"), QStringLiteral("libkf6"), QStringLiteral("libqt6"),
        QStringLiteral("qt6-base"), QStringLiteral("qt-style-kvantum"), QStringLiteral("qt6-style-kvantum"), QStringLiteral("mesa"),
        QStringLiteral("libgl"), QStringLiteral("libegl"), QStringLiteral("xwayland"), QStringLiteral("pipewire"),
        QStringLiteral("wireplumber"), QStringLiteral("security-misc"), QStringLiteral("apparmor"), QStringLiteral("wildos-base"),
        QStringLiteral("wildos-branding"), QStringLiteral("wildos-shell"), QStringLiteral("wildos-ui"), QStringLiteral("wildos-desktop"),
    };
    for (const QString &id : packageIds) {
        const QString name = Transaction::packageName(id);
        for (const QString &prefix : prefixes) {
            if (name.startsWith(prefix)) {
                return true;
            }
        }
    }
    return false;
}
}


Updates::Updates(QObject *parent)
    : QObject(parent)
{
    // Lets polkit ask for the password. Without this hint PackageKit
    // marks every request as non-interactive and polkit refuses at once,
    // so installs failed with "needs the administrator password" and no
    // prompt ever appeared.
    PackageKit::Daemon::global()->setHints(QStringList{QStringLiteral("interactive=true")});
    m_restartNeeded = QFileInfo::exists(QStringLiteral("/run/reboot-required")) || QFileInfo::exists(restartMarker());
    if (available()) {
        readLastChecked();
        listUpdates();
        loadHistory();
    }
}

bool Updates::available() const
{
    auto bus = QDBusConnection::systemBus().interface();
    if (!bus) {
        return false;
    }
    return bus->isServiceRegistered(QStringLiteral("org.freedesktop.PackageKit"))
        || bus->activatableServiceNames().value().contains(QStringLiteral("org.freedesktop.PackageKit"));
}

int Updates::securityCount() const
{
    int count = 0;
    for (const QVariant &update : m_updates) {
        if (update.toMap().value(QStringLiteral("security")).toBool()) {
            ++count;
        }
    }
    return count;
}

void Updates::setState(const QString &state, const QString &error)
{
    m_state = state;
    m_error = error;
    Q_EMIT stateChanged();
}

void Updates::readLastChecked()
{
    auto watcher = new QDBusPendingCallWatcher(Daemon::getTimeSinceAction(Transaction::RoleRefreshCache), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<uint> reply = *watcher;
        // PackageKit answers with the largest uint when it has never checked.
        if (!reply.isError() && reply.value() < 10u * 365 * 24 * 3600) {
            m_lastChecked = QDateTime::currentDateTime().addSecs(-qint64(reply.value()));
        } else {
            m_lastChecked = {};
        }
        Q_EMIT lastCheckedChanged();
    });
}

void Updates::watchProgress(Transaction *transaction)
{
    m_progress = 0;
    m_detail.clear();
    Q_EMIT progressChanged();
    connect(transaction, &Transaction::percentageChanged, this, [this, transaction] {
        // 101 is PackageKit's "unknown".
        const uint pct = transaction->percentage();
        m_progress = pct > 100 ? 0 : int(pct);
        Q_EMIT progressChanged();
    });
    connect(transaction, &Transaction::itemProgress, this, [this](const QString &itemID, Transaction::Status, uint) {
        m_detail = Transaction::packageName(itemID);
        Q_EMIT progressChanged();
    });
    connect(transaction, &Transaction::requireRestart, this, [this](Transaction::Restart type, const QString &) {
        if (type == Transaction::RestartSystem || type == Transaction::RestartSecuritySystem) {
            m_restartNeeded = true;
            Q_EMIT restartNeededChanged();
        }
    });
}


void Updates::check()
{
    if (m_state == QLatin1String("checking") || m_state == QLatin1String("installing")) {
        return;
    }
    setState(QStringLiteral("checking"));
    // Someone asked to check: fetch the lists again even if they are recent.
    Transaction *refresh = Daemon::refreshCache(true);
    watchProgress(refresh);
    connect(refresh, &Transaction::errorCode, this, [this](Transaction::Error error, const QString &) {
        setState(QStringLiteral("error"), errorText(error, false));
    });
    connect(refresh, &Transaction::finished, this, [this](Transaction::Exit status, uint) {
        readLastChecked();
        if (status == Transaction::ExitSuccess) {
            listUpdates();
        } else if (m_state != QLatin1String("error")) {
            setState(QStringLiteral("error"), status == Transaction::ExitCancelled ? QStringLiteral("The check was cancelled.")
                                                                                    : QStringLiteral("Wild OS could not check for updates."));
        }
    });
}

void Updates::listUpdates()
{
    auto collected = std::make_shared<QVariantList>();
    auto ids = std::make_shared<QStringList>();
    if (m_state != QLatin1String("checking")) {
        setState(QStringLiteral("checking"));
    }
    Transaction *list = Daemon::getUpdates();
    connect(list, &Transaction::package, this, [collected, ids](Transaction::Info info, const QString &packageID, const QString &summary) {
        // Blocked updates are held back on purpose and cannot be installed.
        if (info == Transaction::InfoBlocked) {
            return;
        }
        ids->append(packageID);
        collected->append(QVariantMap{
            {QStringLiteral("name"), Transaction::packageName(packageID)},
            {QStringLiteral("version"), Transaction::packageVersion(packageID)},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("security"), info == Transaction::InfoSecurity || info == Transaction::InfoImportant},
        });
    });
    connect(list, &Transaction::errorCode, this, [this](Transaction::Error error, const QString &) {
        setState(QStringLiteral("error"), errorText(error, false));
    });
    connect(list, &Transaction::finished, this, [this, collected, ids](Transaction::Exit status, uint) {
        if (status != Transaction::ExitSuccess) {
            if (m_state != QLatin1String("error")) {
                setState(QStringLiteral("error"), QStringLiteral("Wild OS could not read the list of updates."));
            }
            return;
        }
        // Security fixes first, then by name, so the reason to install now is
        // at the top.
        std::sort(collected->begin(), collected->end(), [](const QVariant &a, const QVariant &b) {
            const auto ma = a.toMap();
            const auto mb = b.toMap();
            if (ma.value(QStringLiteral("security")).toBool() != mb.value(QStringLiteral("security")).toBool()) {
                return ma.value(QStringLiteral("security")).toBool();
            }
            return ma.value(QStringLiteral("name")).toString() < mb.value(QStringLiteral("name")).toString();
        });
        m_updates = *collected;
        m_pending = *ids;
        Q_EMIT updatesChanged();
        setState(m_updates.isEmpty() ? QStringLiteral("uptodate") : QStringLiteral("available"));
    });
}

void Updates::install()
{
    if (m_pending.isEmpty() || m_state == QLatin1String("installing")) {
        return;
    }
    setState(QStringLiteral("installing"));
    Transaction *update = Daemon::updatePackages(m_pending, Transaction::TransactionFlagOnlyTrusted);
    watchProgress(update);
    connect(update, &Transaction::errorCode, this, [this](Transaction::Error error, const QString &) {
        setState(QStringLiteral("error"), errorText(error, true));
    });
    connect(update, &Transaction::finished, this, [this](Transaction::Exit status, uint) {
        loadHistory();
        if (status == Transaction::ExitSuccess) {
            if (needsRestart(m_pending)) {
                QFile marker(restartMarker());
                if (marker.open(QIODevice::WriteOnly)) {
                    marker.close();
                }
                if (!m_restartNeeded) {
                    m_restartNeeded = true;
                    Q_EMIT restartNeededChanged();
                }
            }
            m_updates.clear();
            m_pending.clear();
            Q_EMIT updatesChanged();
            if (QFileInfo::exists(QStringLiteral("/run/reboot-required")) && !m_restartNeeded) {
                m_restartNeeded = true;
                Q_EMIT restartNeededChanged();
            }
            setState(QStringLiteral("installed"));
        } else if (m_state != QLatin1String("error")) {
            setState(QStringLiteral("error"), status == Transaction::ExitCancelled ? QStringLiteral("Installing was cancelled.")
                                                                                    : QStringLiteral("The updates could not be installed."));
        }
    });
}

void Updates::loadHistory()
{
    // PackageKit-Qt listens for the daemon's Transaction signal with a
    // different argument list than PackageKit 1.3 sends, so its history call
    // never reports anything. The history is read over D-Bus directly.
    auto watcher = new QDBusPendingCallWatcher(Daemon::createTransaction(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            return;
        }
        const QString path = reply.value().path();
        QDBusConnection bus = QDBusConnection::systemBus();
        m_historyPath = path;
        m_collected.clear();
        bus.connect(QStringLiteral("org.freedesktop.PackageKit"), path, QStringLiteral("org.freedesktop.PackageKit.Transaction"),
                    QStringLiteral("Transaction"), this,
                    SLOT(onOldTransaction(QDBusObjectPath, QString, bool, uint, uint, QString, uint, QString)));
        bus.connect(QStringLiteral("org.freedesktop.PackageKit"), path, QStringLiteral("org.freedesktop.PackageKit.Transaction"),
                    QStringLiteral("Finished"), this, SLOT(onHistoryFinished(uint, uint)));
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.PackageKit"), path,
                                                   QStringLiteral("org.freedesktop.PackageKit.Transaction"), QStringLiteral("GetOldTransactions"));
        call << uint(50);
        bus.asyncCall(call);
    });
}

void Updates::onOldTransaction(const QDBusObjectPath &, const QString &timespec, bool succeeded, uint role, uint, const QString &data, uint,
                               const QString &)
{
    const auto r = Transaction::Role(role);
    if (r != Transaction::RoleUpdatePackages && r != Transaction::RoleInstallPackages && r != Transaction::RoleRemovePackages) {
        return;
    }
    // data is one "info<TAB>package-id" line per package touched.
    QStringList names;
    const auto lines = data.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString id = line.section(QLatin1Char('\t'), 1, 1);
        if (!id.isEmpty()) {
            names << Transaction::packageName(id);
        }
    }
    m_collected.append(QVariantMap{
        {QStringLiteral("when"), QDateTime::fromString(timespec, Qt::ISODateWithMs).toLocalTime()},
        {QStringLiteral("kind"), r == Transaction::RoleUpdatePackages ? QStringLiteral("update")
                                 : r == Transaction::RoleInstallPackages ? QStringLiteral("install")
                                                                         : QStringLiteral("remove")},
        {QStringLiteral("succeeded"), succeeded},
        {QStringLiteral("packages"), names},
    });
}

void Updates::onHistoryFinished(uint, uint)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.disconnect(QStringLiteral("org.freedesktop.PackageKit"), m_historyPath, QStringLiteral("org.freedesktop.PackageKit.Transaction"),
                   QStringLiteral("Transaction"), this, SLOT(onOldTransaction(QDBusObjectPath, QString, bool, uint, uint, QString, uint, QString)));
    bus.disconnect(QStringLiteral("org.freedesktop.PackageKit"), m_historyPath, QStringLiteral("org.freedesktop.PackageKit.Transaction"),
                   QStringLiteral("Finished"), this, SLOT(onHistoryFinished(uint, uint)));
    std::sort(m_collected.begin(), m_collected.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("when")).toDateTime() > b.toMap().value(QStringLiteral("when")).toDateTime();
    });
    m_history = m_collected;
    m_collected.clear();
    Q_EMIT historyChanged();
}

void Updates::restart()
{
    // Plasma's own restart path, so open apps get the chance to save.
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"),
                                                  QStringLiteral("logoutAndReboot"));
    QDBusConnection::sessionBus().asyncCall(message);
}

QString Updates::errorText(Transaction::Error error, bool installing)
{
    switch (error) {
    case Transaction::ErrorNoNetwork:
    case Transaction::ErrorNoCache:
    case Transaction::ErrorRepoNotAvailable:
    case Transaction::ErrorCannotFetchSources:
        return QStringLiteral("Wild OS could not reach the update servers. Check your internet connection and try again.");
    case Transaction::ErrorNotAuthorized:
        return installing ? QStringLiteral("Installing needs the administrator password, and it was not given.")
                          : QStringLiteral("Checking needs the administrator password, and it was not given.");
    case Transaction::ErrorNoSpaceOnDevice:
        return QStringLiteral("There is not enough free space to install the updates.");
    case Transaction::ErrorLockRequired:
    case Transaction::ErrorCannotGetLock:
        return QStringLiteral("Another app is installing software. Try again when it has finished.");
    case Transaction::ErrorTransactionCancelled:
        return QStringLiteral("The update was cancelled.");
    default:
        return QStringLiteral("Something went wrong while updating. Try again in a few minutes.");
    }
}

namespace
{
// Desktop entries by the names a package is likely to carry: the entry's
// own name, its last reverse-DNS part and the program it runs.
const QHash<QString, QString> &appNames()
{
    static const QHash<QString, QString> names = [] {
        QHash<QString, QString> map;
        const auto services = KService::allServices();
        for (const KService::Ptr &service : services) {
            if (service->noDisplay() || service->name().isEmpty()) {
                continue;
            }
            const QString entry = service->desktopEntryName().toLower();
            map.insert(entry, service->name());
            map.insert(entry.section(QLatin1Char('.'), -1), service->name());
            const QString program = service->exec().section(QLatin1Char(' '), 0, 0).section(QLatin1Char('/'), -1);
            if (!program.isEmpty() && !map.contains(program)) {
                map.insert(program, service->name());
            }
        }
        return map;
    }();
    return names;
}

bool startsWithAny(const QString &name, std::initializer_list<const char *> prefixes)
{
    for (const char *prefix : prefixes) {
        if (name.startsWith(QLatin1String(prefix))) {
            return true;
        }
    }
    return false;
}
}

QVariantMap Updates::describe(const QString &package) const
{
    const QString name = package.toLower();
    auto result = [](const char *group, const QString &title = {}) {
        return QVariantMap{{QStringLiteral("group"), QString::fromLatin1(group)}, {QStringLiteral("title"), title}};
    };
    if (name.startsWith(QLatin1String("wildos"))) {
        return result("wildos");
    }
    if (startsWithAny(name, {"brave-browser", "firefox", "chromium", "google-chrome"})) {
        return result("browser");
    }
    if (startsWithAny(name, {"linux-", "firmware-", "intel-microcode", "amd64-microcode", "grub", "shim", "systemd", "udev", "libc6", "libc-bin",
                             "base-files", "dpkg", "apt", "cryptsetup", "sudo", "openssl", "libssl", "initramfs", "dracut", "plymouth", "polkit",
                             "libpam", "openssh", "dbus", "util-linux", "coreutils", "bash", "tzdata", "ca-certificates", "apparmor", "usbguard",
                             "nftables", "iptables", "efibootmgr", "mokutil"})) {
        return result("core");
    }
    const auto &apps = appNames();
    const auto app = apps.constFind(name);
    if (app != apps.constEnd()) {
        return result("app", app.value());
    }
    if (startsWithAny(name, {"plasma", "kwin", "kde", "libkf6", "kf6", "qml6", "qt6", "libqt6", "sddm", "xdg-desktop-portal", "breeze", "kscreen",
                             "powerdevil", "pipewire", "wireplumber", "libpipewire", "pulseaudio", "mesa", "libgl", "libegl", "libgbm", "libdrm",
                             "libvulkan", "xwayland", "libwayland", "fonts-", "kvantum", "libplasma", "libkscreen", "bluedevil", "bluez"})) {
        return result("desktop");
    }
    if (name.startsWith(QLatin1String("lib"))) {
        return result("library");
    }
    return result("other");
}

QString Updates::releaseNotes() const
{
    // dh_installchangelogs puts a native package's changelog here.
    const QString path = QStringLiteral("/usr/share/doc/wildos-base/changelog.gz");
    if (!QFileInfo::exists(path)) {
        return {};
    }
    QProcess zcat;
    zcat.start(QStringLiteral("zcat"), {path});
    if (!zcat.waitForFinished(3000) || zcat.exitCode() != 0) {
        return {};
    }
    const QString text = QString::fromUtf8(zcat.readAllStandardOutput());
    // The newest entry runs from the first line to the first " -- " sign-off;
    // its bullet lines are what changed.
    QStringList notes;
    const auto lines = text.split(QLatin1Char('\n'));
    for (int i = 1; i < lines.size(); ++i) {
        const QString line = lines.at(i);
        if (line.startsWith(QLatin1String(" -- "))) {
            break;
        }
        static const QRegularExpression bullet(QStringLiteral("^\\s+\\*\\s+"));
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (bullet.match(line).hasMatch()) {
            notes << QString(line).remove(bullet);
        } else if (!notes.isEmpty()) {
            notes.last() += QLatin1Char(' ') + trimmed;
        }
    }
    return notes.join(QLatin1Char('\n'));
}
