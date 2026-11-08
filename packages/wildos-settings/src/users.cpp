#include "users.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <grp.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
const QString accountsServiceName = QStringLiteral("org.freedesktop.Accounts");
const QString userInterface = QStringLiteral("org.freedesktop.Accounts.User");
const QString usersHelper = QStringLiteral("/usr/libexec/wildos/settings-users");

bool isAdmin(const char *name, gid_t gid)
{
    int count = 64;
    QList<gid_t> groups(count);
    if (getgrouplist(name, gid, groups.data(), &count) < 0) {
        groups.resize(count);
        getgrouplist(name, gid, groups.data(), &count);
    }
    groups.resize(count);
    for (gid_t g : std::as_const(groups)) {
        if (struct group *gr = getgrgid(g)) {
            if (qstrcmp(gr->gr_name, "sudo") == 0 || qstrcmp(gr->gr_name, "wheel") == 0) {
                return true;
            }
        }
    }
    return false;
}

QString iconFor(const QString &name, const QString &home, bool own)
{
    QStringList candidates = {QStringLiteral("/var/lib/AccountsService/icons/") + name};
    if (own) {
        candidates.prepend(home + QStringLiteral("/.face.icon"));
    }
    for (const QString &path : std::as_const(candidates)) {
        const QFileInfo info(path);
        if (info.isReadable() && info.size() > 0) {
            QUrl url = QUrl::fromLocalFile(path);
            // The picture keeps its path when it changes; the timestamp
            // keeps the image cache from showing the old one.
            url.setQuery(QStringLiteral("v=") + QString::number(info.lastModified().toMSecsSinceEpoch()));
            return url.toString();
        }
    }
    return {};
}

// SDDM reads its drop-ins in order, then sddm.conf, and the last
// [Autologin] User= wins (the same reading as boot-handoff).
QString sddmAutoLogin()
{
    QStringList files;
    for (const QString &dir : {QStringLiteral("/usr/lib/sddm/sddm.conf.d"), QStringLiteral("/etc/sddm.conf.d")}) {
        const auto names = QDir(dir).entryList({QStringLiteral("*.conf")}, QDir::Files, QDir::Name);
        for (const QString &n : names) {
            files << dir + QLatin1Char('/') + n;
        }
    }
    files << QStringLiteral("/etc/sddm.conf");
    QString user;
    for (const QString &path : std::as_const(files)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString section;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                section = line;
            } else if (section == QLatin1String("[Autologin]") && line.startsWith(QLatin1String("User="))) {
                user = line.mid(5).trimmed();
            }
        }
    }
    return user;
}

// passwd on a pseudo-terminal, answering its prompts. Returns an empty
// string on success, otherwise what passwd said.
QString runPasswd(const QByteArray &current, const QByteArray &next)
{
    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, nullptr);
    if (pid < 0) {
        return QStringLiteral("Wild OS could not start the password change.");
    }
    if (pid == 0) {
        setenv("LC_ALL", "C", 1);
        setenv("LANGUAGE", "C", 1);
        execl("/usr/bin/passwd", "passwd", static_cast<char *>(nullptr));
        _exit(127);
    }

    QByteArray pending;
    QByteArray all;
    int newPrompts = 0;
    bool gaveUp = false;
    while (true) {
        pollfd fd{master, POLLIN, 0};
        const int ready = poll(&fd, 1, 15000);
        if (ready <= 0) {
            gaveUp = ready == 0;
            break;
        }
        char buffer[512];
        const ssize_t n = read(master, buffer, sizeof buffer);
        if (n <= 0) {
            break;
        }
        pending.append(buffer, n);
        all.append(buffer, n);
        const QByteArray prompt = pending.trimmed().toLower();
        if (!prompt.endsWith("password:")) {
            continue;
        }
        const QByteArray last = prompt.mid(prompt.lastIndexOf('\n') + 1);
        QByteArray answer;
        if (last.contains("new") || last.contains("retype") || last.contains("again")) {
            // A third "new password" prompt means the first was turned
            // down; passwd has already said why.
            if (++newPrompts > 2) {
                gaveUp = true;
                break;
            }
            answer = next;
        } else {
            answer = current;
        }
        answer.append('\n');
        if (write(master, answer.constData(), size_t(answer.size())) < 0) {
            break;
        }
        pending.clear();
    }
    if (gaveUp) {
        kill(pid, SIGTERM);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    close(master);
    if (!gaveUp && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return {};
    }

    const QString output = QString::fromUtf8(all);
    if (output.contains(QLatin1String("Authentication token manipulation error"), Qt::CaseInsensitive)
        || output.contains(QLatin1String("Authentication failure"), Qt::CaseInsensitive)) {
        if (newPrompts == 0) {
            return QStringLiteral("The current password isn't right.");
        }
    }
    static const QRegularExpression bad(QStringLiteral("BAD PASSWORD:\\s*(.+)"));
    const auto match = bad.match(output);
    if (match.hasMatch()) {
        QString reason = match.captured(1).trimmed();
        reason[0] = reason[0].toUpper();
        if (!reason.endsWith(QLatin1Char('.'))) {
            reason += QLatin1Char('.');
        }
        return QStringLiteral("Wild OS turned down the new password. ") + reason;
    }
    if (output.contains(QLatin1String("same"), Qt::CaseInsensitive) || output.contains(QLatin1String("unchanged"), Qt::CaseInsensitive)) {
        return QStringLiteral("Choose a password you have not used for this account before.");
    }
    return QStringLiteral("The password was not changed.");
}
}

Users::Users(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    m_accountsService = bus.isConnected() && bus.interface()
        && (bus.interface()->isServiceRegistered(accountsServiceName) || bus.interface()->activatableServiceNames().value().contains(accountsServiceName));
    bus.connect(accountsServiceName, userPath(), userInterface, QStringLiteral("Changed"), this, SLOT(reload()));
    bus.connect(accountsServiceName, QStringLiteral("/org/freedesktop/Accounts"), accountsServiceName, QStringLiteral("UserAdded"), this, SLOT(reload()));
    bus.connect(accountsServiceName, QStringLiteral("/org/freedesktop/Accounts"), accountsServiceName, QStringLiteral("UserDeleted"), this, SLOT(reload()));
    reload();
}

QString Users::userPath() const
{
    return QStringLiteral("/org/freedesktop/Accounts/User%1").arg(getuid());
}

void Users::reload()
{
    const uid_t self = getuid();
    m_me.clear();
    m_others.clear();
    m_adminCount = 0;

    // Real people: the ids useradd hands out, with a shell to sign in with.
    setpwent();
    while (struct passwd *pw = getpwent()) {
        const bool own = pw->pw_uid == self;
        const QString shell = QString::fromUtf8(pw->pw_shell);
        if (!own && (pw->pw_uid < 1000 || pw->pw_uid >= 60000 || shell.endsWith(QLatin1String("nologin")) || shell.endsWith(QLatin1String("/false")))) {
            continue;
        }
        const QString name = QString::fromUtf8(pw->pw_name);
        const QString full = QString::fromUtf8(pw->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
        const bool admin = isAdmin(pw->pw_name, pw->pw_gid);
        if (admin && pw->pw_uid >= 1000) {
            ++m_adminCount;
        }
        const QVariantMap user = {
            {QStringLiteral("userName"), name},
            {QStringLiteral("realName"), full.isEmpty() ? name : full},
            {QStringLiteral("icon"), iconFor(name, QString::fromUtf8(pw->pw_dir), own)},
            {QStringLiteral("admin"), admin},
            {QStringLiteral("uid"), uint(pw->pw_uid)},
        };
        if (own) {
            m_me = user;
        } else {
            m_others << user;
        }
    }
    endpwent();
    std::sort(m_others.begin(), m_others.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("realName")).toString().localeAwareCompare(b.toMap().value(QStringLiteral("realName")).toString()) < 0;
    });
    m_autoLogin = sddmAutoLogin();

    if (m_accountsService) {
        auto message = QDBusMessage::createMethodCall(accountsServiceName, userPath(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        message << userInterface << QStringLiteral("PasswordHint");
        const QDBusMessage reply = QDBusConnection::systemBus().call(message, QDBus::Block, 2000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            m_me.insert(QStringLiteral("hint"), reply.arguments().constFirst().value<QDBusVariant>().variant().toString());
        }
    }
    Q_EMIT changed();
}

void Users::setBusy(const QString &busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        Q_EMIT busyChanged();
    }
}

void Users::callUser(const QString &method, const QVariant &argument, const QString &what)
{
    auto message = QDBusMessage::createMethodCall(accountsServiceName, userPath(), userInterface, method);
    message << argument;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 60000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, what] {
        watcher->deleteLater();
        QDBusPendingReply<> reply = *watcher;
        reload();
        if (reply.isError()) {
            Q_EMIT failed(what, reply.error().type() == QDBusError::AccessDenied ? QStringLiteral("Wild OS was not allowed to change it.")
                                                                                 : QStringLiteral("Wild OS could not save the change."));
        } else {
            Q_EMIT done(what, QString());
        }
    });
}

void Users::setRealName(const QString &name)
{
    const QString clean = name.simplified();
    if (clean.isEmpty() || clean.contains(QLatin1Char(':')) || clean.contains(QLatin1Char(','))) {
        Q_EMIT failed(QStringLiteral("name"), QStringLiteral("A name can't be empty or contain a colon or comma."));
        return;
    }
    callUser(QStringLiteral("SetRealName"), clean, QStringLiteral("name"));
}

QSize Users::pictureSize(const QString &file) const
{
    QImageReader reader(QUrl(file).isLocalFile() ? QUrl(file).toLocalFile() : file);
    reader.setAutoTransform(true);
    QSize size = reader.size();
    // Rotated photos report their stored size; the transform swaps it.
    if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
        size.transpose();
    }
    return size;
}

void Users::setPicture(const QString &file, qreal x, qreal y, qreal size)
{
    const QString path = QUrl(file).isLocalFile() ? QUrl(file).toLocalFile() : file;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        Q_EMIT failed(QStringLiteral("picture"), QStringLiteral("That file isn't a picture Wild OS can open."));
        return;
    }
    const QRect square = QRectF(x, y, size, size).toRect().intersected(image.rect());
    // 512px is plenty for the 200px sign-in picture on a high-density screen
    // and well under AccountsService's size limit.
    const QImage face = image.copy(square).scaled(512, 512, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/wildos-settings");
    QDir().mkpath(dir);
    const QString staged = dir + QStringLiteral("/account-picture.png");
    if (!face.save(staged, "PNG")) {
        Q_EMIT failed(QStringLiteral("picture"), QStringLiteral("Wild OS could not save the picture."));
        return;
    }
    // ~/.face.icon is what KDE parts and the sign-in screen read for the
    // account's own picture; AccountsService keeps the copy other accounts
    // and the sign-in screen see.
    const QString faceIcon = QDir::homePath() + QStringLiteral("/.face.icon");
    QFile::remove(faceIcon);
    QFile::copy(staged, faceIcon);
    QFile::setPermissions(faceIcon, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    if (m_accountsService) {
        callUser(QStringLiteral("SetIconFile"), staged, QStringLiteral("picture"));
    } else {
        reload();
        Q_EMIT done(QStringLiteral("picture"), QString());
    }
}

void Users::clearPicture()
{
    QFile::remove(QDir::homePath() + QStringLiteral("/.face.icon"));
    if (m_accountsService) {
        callUser(QStringLiteral("SetIconFile"), QString(), QStringLiteral("picture"));
    } else {
        reload();
        Q_EMIT done(QStringLiteral("picture"), QString());
    }
}

void Users::changePassword(const QString &current, const QString &next, const QString &hint)
{
    setBusy(QStringLiteral("password"));
    QPointer<Users> self(this);
    QThread *thread = QThread::create([self, current = current.toUtf8(), next = next.toUtf8(), hint] {
        const QString error = runPasswd(current, next);
        QMetaObject::invokeMethod(qApp, [self, error, hint] {
            if (!self) {
                return;
            }
            self->setBusy({});
            if (!error.isEmpty()) {
                Q_EMIT self->failed(QStringLiteral("password"), error);
                return;
            }
            if (self->m_accountsService && hint != self->m_me.value(QStringLiteral("hint")).toString()) {
                self->callUser(QStringLiteral("SetPasswordHint"), hint, QStringLiteral("password"));
            }
            Q_EMIT self->done(QStringLiteral("password"), QStringLiteral("Your password was changed. Use the new one next time you sign in."));
        });
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void Users::helper(const QString &what, const QStringList &arguments, const QByteArray &input, const QString &success)
{
    setBusy(what);
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, what, success](int code, QProcess::ExitStatus status) {
        const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
        process->deleteLater();
        setBusy({});
        reload();
        if (status == QProcess::NormalExit && code == 0) {
            Q_EMIT done(what, success);
        } else if (code == 126 || code == 127) {
            Q_EMIT failed(what, QStringLiteral("An administrator's password is needed for this."));
        } else {
            Q_EMIT failed(what, error.isEmpty() ? QStringLiteral("Wild OS could not make the change.") : error);
        }
    });
    process->start(QStringLiteral("pkexec"), QStringList{usersHelper} + arguments);
    if (!input.isEmpty()) {
        process->write(input);
    }
    process->closeWriteChannel();
}

void Users::addUser(const QString &fullName, const QString &userName, const QString &password, bool admin)
{
    helper(QStringLiteral("add"), {QStringLiteral("add"), userName, admin ? QStringLiteral("admin") : QStringLiteral("standard"), fullName.simplified()},
           password.toUtf8() + '\n', fullName.simplified() + QStringLiteral(" can now sign in to this computer."));
}

void Users::removeUser(const QString &userName)
{
    helper(QStringLiteral("remove"), {QStringLiteral("remove"), userName}, {}, QStringLiteral("The account and its files were removed."));
}

void Users::setAdmin(const QString &userName, bool admin)
{
    helper(QStringLiteral("type"), {QStringLiteral("admin"), userName, admin ? QStringLiteral("on") : QStringLiteral("off")}, {},
           admin ? QStringLiteral("The account is now an administrator.") : QStringLiteral("The account is now a standard user."));
}

void Users::setAutoLogin(bool on)
{
    helper(QStringLiteral("autologin"), {QStringLiteral("autologin"), on ? m_me.value(QStringLiteral("userName")).toString() : QStringLiteral("off")}, {},
           on ? QStringLiteral("This computer will sign you in when it starts.") : QStringLiteral("This computer will ask for your password when it starts."));
}

QString Users::suggestUserName(const QString &fullName) const
{
    QString first = fullName.simplified().section(QLatin1Char(' '), 0, 0).normalized(QString::NormalizationForm_KD).toLower();
    first.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));
    first.remove(QRegularExpression(QStringLiteral("^[^a-z_]+")));
    first.truncate(32);
    if (first.isEmpty()) {
        return {};
    }
    QString candidate = first;
    for (int i = 2; getpwnam(candidate.toUtf8().constData()) != nullptr; ++i) {
        candidate = first + QString::number(i);
    }
    return candidate;
}

QString Users::checkUserName(const QString &userName) const
{
    static const QRegularExpression valid(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    if (userName.isEmpty()) {
        return QStringLiteral("Choose a user name.");
    }
    if (!valid.match(userName).hasMatch()) {
        return QStringLiteral("Use small letters, numbers, - and _ only, starting with a letter.");
    }
    if (getpwnam(userName.toUtf8().constData()) != nullptr || getgrnam(userName.toUtf8().constData()) != nullptr) {
        return QStringLiteral("That user name is taken.");
    }
    return {};
}
