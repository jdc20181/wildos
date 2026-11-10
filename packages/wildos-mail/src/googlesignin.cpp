#include "googlesignin.h"
#include "mailengine.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QEventLoop>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <libedataserver/libedataserver.h>

namespace
{
// Adding Gmail to Mail asks for mail, calendars, contacts and tasks in one
// go, so the account's calendar comes with it; Calendar's own sign-in (for
// an account Mail had before, whose token covers mail only) asks for the
// rest. Calendars, contacts and tasks are what evolution-data-server's
// Google module reads.
const QString mailScope = QStringLiteral("https://mail.google.com/");
const QString calendarScope = QStringLiteral("https://www.googleapis.com/auth/calendar https://www.googleapis.com/auth/carddav https://www.googleapis.com/auth/tasks");

struct GoogleApp {
    QString clientId;
    QString clientSecret;
    QString authUri;
    QString tokenUri;
    QString redirectUri;
};

// evolution-data-server's Google app, read from the library itself, so
// Mail always uses the registration the installed engine was built with.
const GoogleApp &googleApp()
{
    static const GoogleApp app = []() {
        GoogleApp a;
        GError *error = nullptr;
        ESource *source = e_source_new(nullptr, nullptr, &error);
        g_clear_error(&error);
        EOAuth2Services *services = e_oauth2_services_new();
        GSList *list = e_oauth2_services_list(services);
        for (GSList *l = list; l && source; l = l->next) {
            auto *service = E_OAUTH2_SERVICE(l->data);
            if (g_strcmp0(e_oauth2_service_get_name(service), "Google") != 0) {
                continue;
            }
            a.clientId = QString::fromUtf8(e_oauth2_service_get_client_id(service, source));
            a.clientSecret = QString::fromUtf8(e_oauth2_service_get_client_secret(service, source));
            a.authUri = QString::fromUtf8(e_oauth2_service_get_authentication_uri(service, source));
            a.tokenUri = QString::fromUtf8(e_oauth2_service_get_refresh_uri(service, source));
            a.redirectUri = QString::fromUtf8(e_oauth2_service_get_redirect_uri(service, source));
        }
        g_slist_free_full(list, g_object_unref);
        g_object_unref(services);
        g_clear_object(&source);
        return a;
    }();
    return app;
}

QByteArray randomUrlSafe(int bytes)
{
    QByteArray raw(bytes, Qt::Uninitialized);
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(raw.data()), reinterpret_cast<quint32 *>(raw.data() + (bytes / 4) * 4));
    return raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

// A token request, waited for here: Camel asks for tokens from its own
// threads, so each call has its own network access and event loop.
QByteArray post(const QString &uri, const QByteArray &body, QString *error)
{
    QNetworkAccessManager network;
    network.setCookieJar(nullptr);
    QNetworkRequest request{QUrl(uri)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(30000);
    QNetworkReply *reply = network.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError && error) {
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        const QString detail = json.value(QStringLiteral("error_description")).toString();
        *error = detail.isEmpty() ? reply->errorString() : detail;
    }
    const bool ok = reply->error() == QNetworkReply::NoError;
    delete reply;
    return ok ? data : QByteArray();
}
}

GoogleSignIn *GoogleSignIn::instance()
{
    static GoogleSignIn *signIn = new GoogleSignIn();
    return signIn;
}

GoogleSignIn *GoogleSignIn::create(QQmlEngine *, QJSEngine *)
{
    GoogleSignIn *signIn = instance();
    QJSEngine::setObjectOwnership(signIn, QJSEngine::CppOwnership);
    return signIn;
}

GoogleSignIn::GoogleSignIn(QObject *parent)
    : QObject(parent)
    , m_network(nullptr)
{
}

bool GoogleSignIn::isRedirect(const QString &url)
{
    const QString redirect = googleApp().redirectUri;
    return !redirect.isEmpty() && url.startsWith(redirect);
}

void GoogleSignIn::startCalendar(const QString &email)
{
    m_scope = calendarScope;
    begin(email);
}

void GoogleSignIn::start(const QString &email)
{
    m_scope = mailScope + QLatin1Char(' ') + calendarScope;
    begin(email);
}

void GoogleSignIn::begin(const QString &email)
{
    const GoogleApp &app = googleApp();
    if (app.clientId.isEmpty()) {
        Q_EMIT failed(tr("Google sign-in isn't available on this PC."));
        return;
    }
    m_email = email.trimmed();
    m_state = QString::fromLatin1(randomUrlSafe(24));
    // PKCE: only the copy of Mail that started the sign-in can finish it.
    m_verifier = randomUrlSafe(48);
    const QByteArray challenge = QCryptographicHash::hash(m_verifier, QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    QUrl url(app.authUri);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), app.clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), app.redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), m_scope);
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("state"), m_state);
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    if (!m_email.isEmpty()) {
        query.addQueryItem(QStringLiteral("login_hint"), m_email);
    }
    url.setQuery(query);
    m_url = url.toString(QUrl::FullyEncoded);
    qCInfo(mailLog) << "opening Google sign-in in the browser";
    Q_EMIT waitingChanged();
    QDesktopServices::openUrl(QUrl(m_url));
}

void GoogleSignIn::reopen()
{
    if (!m_url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_url));
    }
}

void GoogleSignIn::cancel()
{
    m_state.clear();
    m_verifier.clear();
    m_url.clear();
    Q_EMIT waitingChanged();
}

bool GoogleSignIn::handleRedirect(const QString &address)
{
    if (!isRedirect(address)) {
        return false;
    }
    qCInfo(mailLog) << "Google sent the browser back to Mail";
    const QUrlQuery query(QUrl(address).query());
    const QString state = query.queryItemValue(QStringLiteral("state"));
    const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
    const QString error = query.queryItemValue(QStringLiteral("error"));
    // Only the sign-in this copy of Mail started.
    if (m_state.isEmpty() || state != m_state) {
        qCWarning(mailLog) << "ignored a Google answer for a sign-in this Mail didn't start";
        return true;
    }
    const QByteArray verifier = m_verifier;
    const QString email = m_email;
    cancel();
    if (!error.isEmpty() || code.isEmpty()) {
        Q_EMIT failed(error == QLatin1String("access_denied") ? tr("Google sign-in was cancelled.") : tr("Google didn't sign you in: %1").arg(error));
        return true;
    }

    const GoogleApp &app = googleApp();
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("client_id"), app.clientId);
    body.addQueryItem(QStringLiteral("client_secret"), app.clientSecret);
    body.addQueryItem(QStringLiteral("redirect_uri"), app.redirectUri);
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("code_verifier"), QString::fromLatin1(verifier));
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        m_network->setCookieJar(nullptr);
    }
    QNetworkRequest request{QUrl(app.tokenUri)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->post(request, body.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, email]() {
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        const QString refresh = json.value(QStringLiteral("refresh_token")).toString();
        if (reply->error() != QNetworkReply::NoError || refresh.isEmpty()) {
            const QString detail = json.value(QStringLiteral("error_description")).toString();
            qCWarning(mailLog) << "Google token exchange failed:" << (detail.isEmpty() ? reply->errorString() : detail);
            Q_EMIT failed(tr("Google didn't finish signing you in. %1").arg(detail.isEmpty() ? reply->errorString() : detail));
            return;
        }
        qCInfo(mailLog) << "Google sign-in finished";
        Q_EMIT signedIn(email, refresh);
    });
    return true;
}

bool GoogleSignIn::accessToken(const QString &refreshToken, QString *token, int *expiresIn, QString *error)
{
    const GoogleApp &app = googleApp();
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    body.addQueryItem(QStringLiteral("client_id"), app.clientId);
    body.addQueryItem(QStringLiteral("client_secret"), app.clientSecret);
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    const QByteArray data = post(app.tokenUri, body.query(QUrl::FullyEncoded).toUtf8(), error);
    const QJsonObject json = QJsonDocument::fromJson(data).object();
    *token = json.value(QStringLiteral("access_token")).toString();
    *expiresIn = json.value(QStringLiteral("expires_in")).toInt(3600);
    return !token->isEmpty();
}
