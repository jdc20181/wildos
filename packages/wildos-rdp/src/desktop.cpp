#include "desktop.h"
#include "client.h"

#include <QLocale>
#include <QCryptographicHash>
#include <QDateTime>
#include <QSslCertificate>

#include <freerdp/autodetect.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/disp.h>
#include <freerdp/settings.h>

#include <chrono>
#include <cstring>
#include <vector>

namespace
{
DesktopSession *sessionOf(freerdp *instance)
{
    if (!instance || !instance->context) {
        return nullptr;
    }
    Client *client = reinterpret_cast<WrdpContext *>(instance->context)->client;
    return client ? qobject_cast<DesktopSession *>(client->parent()) : nullptr;
}

QString colonHex(const QByteArray &bytes)
{
    QStringList parts;
    for (const char b : bytes) {
        parts.append(QStringLiteral("%1").arg(quint8(b), 2, 16, QLatin1Char('0')).toUpper());
    }
    return parts.join(QLatin1Char(':'));
}

// What the person needs to check a certificate: the name it gives, who
// issued it, when it runs out, and its SHA-256 fingerprint.
QVariantMap certificateInfo(const char *host, UINT16 port, const char *commonName, const char *subject, const char *issuer,
                            const char *fingerprint, DWORD flags)
{
    QVariantMap info;
    info.insert(QStringLiteral("host"), QString::fromUtf8(host));
    info.insert(QStringLiteral("port"), int(port));
    info.insert(QStringLiteral("commonName"), QString::fromUtf8(commonName ? commonName : ""));
    info.insert(QStringLiteral("subject"), QString::fromUtf8(subject ? subject : ""));
    info.insert(QStringLiteral("issuer"), QString::fromUtf8(issuer ? issuer : ""));
    info.insert(QStringLiteral("mismatch"), bool(flags & VERIFY_CERT_FLAG_MISMATCH));
    if ((flags & VERIFY_CERT_FLAG_FP_IS_PEM) && fingerprint) {
        const QList<QSslCertificate> certs = QSslCertificate::fromData(QByteArray(fingerprint), QSsl::Pem);
        if (!certs.isEmpty()) {
            const QSslCertificate &c = certs.first();
            info.insert(QStringLiteral("fingerprint"), colonHex(c.digest(QCryptographicHash::Sha256)));
            info.insert(QStringLiteral("validUntil"), QLocale::system().toString(c.expiryDate().toLocalTime().date(), QLocale::ShortFormat));
            info.insert(QStringLiteral("expired"), c.expiryDate() < QDateTime::currentDateTimeUtc());
            info.insert(QStringLiteral("selfSigned"), c.isSelfSigned());
            const QString cn = c.subjectInfo(QSslCertificate::CommonName).value(0);
            if (!cn.isEmpty()) {
                info.insert(QStringLiteral("commonName"), cn);
            }
            const QString issuerName = c.issuerInfo(QSslCertificate::CommonName).value(0);
            if (!issuerName.isEmpty()) {
                info.insert(QStringLiteral("issuer"), issuerName);
            }
        }
    } else if (fingerprint) {
        info.insert(QStringLiteral("fingerprint"), QString::fromUtf8(fingerprint).toUpper());
    }
    return info;
}

void replace(char **slot, const QByteArray &value)
{
    if (!slot) {
        return;
    }
    free(*slot);
    *slot = value.isEmpty() ? nullptr : strdup(value.constData());
}
}

// FreeRDP's questions, on its thread: each one is put to the GUI thread
// as a signal, and the thread waits for the answer.
struct DesktopCallbacks {
    static DWORD verifyCertificate(freerdp *instance, const char *host, UINT16 port, const char *commonName, const char *subject,
                                   const char *issuer, const char *fingerprint, DWORD flags)
    {
        DesktopSession *self = sessionOf(instance);
        if (!self) {
            return 0;
        }
        return ask(self, certificateInfo(host, port, commonName, subject, issuer, fingerprint, flags), false);
    }

    static DWORD verifyChangedCertificate(freerdp *instance, const char *host, UINT16 port, const char *commonName, const char *subject,
                                          const char *issuer, const char *fingerprint, const char *, const char *,
                                          const char *oldFingerprint, DWORD flags)
    {
        DesktopSession *self = sessionOf(instance);
        if (!self) {
            return 0;
        }
        QVariantMap info = certificateInfo(host, port, commonName, subject, issuer, fingerprint, flags);
        const QVariantMap old = certificateInfo(host, port, "", "", "", oldFingerprint, flags);
        info.insert(QStringLiteral("previousFingerprint"), old.value(QStringLiteral("fingerprint")));
        return ask(self, info, true);
    }

    static DWORD ask(DesktopSession *self, const QVariantMap &info, bool changed)
    {
        std::unique_lock lock(self->m_lock);
        self->m_certificateAnswer = -1;
        QMetaObject::invokeMethod(self, [self, info, changed] { Q_EMIT self->certificateNeeded(info, changed); }, Qt::QueuedConnection);
        self->m_wait.wait(lock, [self] { return self->m_certificateAnswer >= 0 || self->m_stopping; });
        // 1 keeps it in FreeRDP's store, 2 trusts it for this session only.
        return self->m_stopping ? 0 : DWORD(self->m_certificateAnswer);
    }

    static BOOL authenticate(freerdp *instance, char **username, char **password, char **domain, rdp_auth_reason)
    {
        DesktopSession *self = sessionOf(instance);
        if (!self) {
            return FALSE;
        }
        if (username && *username && **username && password && *password && **password) {
            return TRUE;
        }
        std::unique_lock lock(self->m_lock);
        self->m_credentialsAnswered = false;
        const QString user = QString::fromUtf8(username && *username ? *username : "");
        const QString dom = QString::fromUtf8(domain && *domain ? *domain : "");
        QMetaObject::invokeMethod(self, [self, user, dom] { Q_EMIT self->credentialsNeeded(user, dom, QStringLiteral("first")); }, Qt::QueuedConnection);
        self->m_wait.wait(lock, [self] { return self->m_credentialsAnswered || self->m_stopping; });
        if (self->m_stopping || !self->m_credentialsOk) {
            return FALSE;
        }
        replace(username, self->m_user);
        replace(password, self->m_password);
        if (domain && !self->m_domain.isEmpty()) {
            replace(domain, self->m_domain);
        }
        self->m_password.fill('\0');
        self->m_password.clear();
        return TRUE;
    }

    static SSIZE_T retry(freerdp *instance, const char *, size_t current, void *)
    {
        DesktopSession *self = sessionOf(instance);
        if (!self) {
            return -1;
        }
        std::unique_lock lock(self->m_lock);
        if (self->m_stopTrying || self->m_stopping || int(current) >= self->m_attempts) {
            return -1;
        }
        self->m_tryNow = false;
        const int attempt = int(current) + 1;
        const int attempts = self->m_attempts;
        QMetaObject::invokeMethod(self, [self, attempt, attempts] { Q_EMIT self->reconnecting(attempt, attempts); }, Qt::QueuedConnection);
        // A moment for a network to come back before each try; "Try now"
        // ends the wait.
        const auto wait = std::chrono::milliseconds(current == 0 ? 1500 : 4000);
        self->m_wait.wait_for(lock, wait, [self] { return self->m_tryNow || self->m_stopTrying || self->m_stopping; });
        return self->m_stopTrying || self->m_stopping ? -1 : 0;
    }

    static void channelConnected(void *ctx, const ChannelConnectedEventArgs *e)
    {
        auto *context = static_cast<rdpContext *>(ctx);
        DesktopSession *self = sessionOf(context->instance);
        if (self && strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
            std::lock_guard lock(self->m_lock);
            self->m_disp = static_cast<DispClientContext *>(e->pInterface);
        }
    }

    static void channelDisconnected(void *ctx, const ChannelDisconnectedEventArgs *e)
    {
        auto *context = static_cast<rdpContext *>(ctx);
        DesktopSession *self = sessionOf(context->instance);
        if (self && strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
            std::lock_guard lock(self->m_lock);
            self->m_disp = nullptr;
        }
    }
};

DesktopSession::DesktopSession(QObject *parent)
    : QObject(parent)
    , m_client(new Client(this))
{
    connect(m_client, &Client::connected, this, &DesktopSession::connected);
    connect(m_client, &Client::connectionBack, this, &DesktopSession::reconnected);
    connect(m_client, &Client::primaryDamaged, this, &DesktopSession::frameChanged);
    connect(m_client, &Client::pointerImage, this, &DesktopSession::pointerImage);
    connect(m_client, &Client::pointerHidden, this, &DesktopSession::pointerHidden);
    connect(m_client, &Client::pointerDefault, this, &DesktopSession::pointerDefault);
    connect(m_client, &Client::finished, this, [this](const QString &error, bool authFailed) {
        Q_EMIT finished(m_stopping ? QString() : error, authFailed);
    });
}

DesktopSession::~DesktopSession()
{
    stop();
}

bool DesktopSession::configure(const QList<QByteArray> &args, QString *error)
{
    rdpContext *context = m_client->context();
    if (!context) {
        *error = QStringLiteral("FreeRDP could not be set up");
        return false;
    }
    std::vector<QByteArray> storage;
    storage.push_back(QByteArrayLiteral("wildos-remote"));
    for (const QByteArray &a : args) {
        storage.push_back(a);
    }
    std::vector<char *> argv;
    for (QByteArray &a : storage) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    rdpSettings *settings = context->settings;
    const int status = freerdp_client_settings_parse_command_line(settings, int(argv.size() - 1), argv.data(), FALSE);
    for (QByteArray &a : storage) {
        a.fill('\0');
    }
    if (status != 0) {
        *error = QStringLiteral("the connection settings could not be read (%1)").arg(status);
        return false;
    }
    if (freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode)) {
        *error = QStringLiteral("a desktop session can't open a single app (/app)");
        return false;
    }
    // The certificate itself, so its dates and issuer can be shown.
    (void)freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, TRUE);
    (void)freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, TRUE);
    (void)freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries, UINT32(m_attempts));
    const char *password = freerdp_settings_get_string(settings, FreeRDP_Password);
    m_passwordGiven = password && *password;

    freerdp *instance = context->instance;
    instance->VerifyCertificateEx = DesktopCallbacks::verifyCertificate;
    instance->VerifyChangedCertificateEx = DesktopCallbacks::verifyChangedCertificate;
    instance->AuthenticateEx = DesktopCallbacks::authenticate;
    instance->RetryDialog = DesktopCallbacks::retry;
    (void)PubSub_SubscribeChannelConnected(context->pubSub, DesktopCallbacks::channelConnected);
    (void)PubSub_SubscribeChannelDisconnected(context->pubSub, DesktopCallbacks::channelDisconnected);
    return true;
}

void DesktopSession::setDisplay(const QSize &physical, double scale)
{
    m_client->setDisplay(physical, scale);
}

void DesktopSession::setMonitors(const QList<QRect> &monitors, double scale)
{
    rdpSettings *settings = m_client->context()->settings;
    QRect bounds;
    for (const QRect &m : monitors) {
        bounds |= m;
    }
    if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, nullptr, size_t(monitors.size()))) {
        return;
    }
    for (int i = 0; i < monitors.size(); ++i) {
        auto *m = static_cast<rdpMonitor *>(freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, size_t(i)));
        if (!m) {
            return;
        }
        // Windows wants the primary monitor at 0,0.
        const QRect r = monitors[i].translated(-monitors.first().topLeft());
        m->x = r.x();
        m->y = r.y();
        m->width = r.width();
        m->height = r.height();
        m->is_primary = i == 0;
        m->orig_screen = UINT32(i);
    }
    (void)freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, UINT32(monitors.size()));
    (void)freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE);
    (void)freerdp_settings_set_bool(settings, FreeRDP_ForceMultimon, TRUE);
    m_client->setDisplay(bounds.size(), scale);
}

void DesktopSession::setReconnectAttempts(int attempts)
{
    m_attempts = attempts;
    (void)freerdp_settings_set_uint32(m_client->context()->settings, FreeRDP_AutoReconnectMaxRetries, UINT32(attempts));
}

void DesktopSession::start()
{
    m_stopping = false;
    m_client->start();
}

void DesktopSession::stop()
{
    {
        std::lock_guard lock(m_lock);
        m_stopping = true;
    }
    m_wait.notify_all();
    m_client->stop();
}

void DesktopSession::wake()
{
    m_wait.notify_all();
}

void DesktopSession::answerCertificate(bool accept, bool remember)
{
    {
        std::lock_guard lock(m_lock);
        m_certificateAnswer = accept ? (remember ? 1 : 2) : 0;
    }
    wake();
}

void DesktopSession::answerCredentials(bool ok, const QString &user, const QString &domain, const QString &password)
{
    {
        std::lock_guard lock(m_lock);
        m_credentialsOk = ok;
        m_user = user.toUtf8();
        m_domain = domain.toUtf8();
        m_password = password.toUtf8();
        m_credentialsAnswered = true;
    }
    wake();
}

void DesktopSession::tryNow()
{
    {
        std::lock_guard lock(m_lock);
        m_tryNow = true;
    }
    wake();
}

void DesktopSession::stopTrying()
{
    {
        std::lock_guard lock(m_lock);
        m_stopTrying = true;
    }
    wake();
}

QImage DesktopSession::frame() const
{
    QImage out;
    m_client->withPrimary([&out](const uchar *data, int stride, QSize size) {
        out = QImage(data, size.width(), size.height(), stride, QImage::Format_RGB32).copy();
    });
    return out;
}

QSize DesktopSession::desktopSize() const
{
    return m_client->desktopSize();
}

int DesktopSession::latency() const
{
    rdpContext *context = m_client->context();
    if (!context || !context->autodetect) {
        return 0;
    }
    return int(context->autodetect->netCharAverageRTT);
}

bool DesktopSession::resize(const QSize &physical, double scale)
{
    std::lock_guard lock(m_lock);
    if (!m_disp || !m_disp->SendMonitorLayout) {
        return false;
    }
    DISPLAY_CONTROL_MONITOR_LAYOUT layout = {};
    layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
    // Windows takes even widths, and nothing under 200 pixels.
    layout.Width = UINT32(std::max(200, physical.width() & ~1));
    layout.Height = UINT32(std::max(200, physical.height()));
    layout.Orientation = ORIENTATION_LANDSCAPE;
    layout.DesktopScaleFactor = UINT32(qBound(100.0, scale * 100.0 + 0.5, 500.0));
    layout.DeviceScaleFactor = scale >= 1.6 ? 180 : scale >= 1.2 ? 140 : 100;
    return m_disp->SendMonitorLayout(m_disp, 1, &layout) == CHANNEL_RC_OK;
}
