#include "storeapp.h"

#include "catalog.h"
#include "flatpakbackend.h"

#include <AppStreamQt/developer.h>
#include <AppStreamQt/icon.h>
#include <AppStreamQt/image.h>
#include <AppStreamQt/launchable.h>
#include <AppStreamQt/release-list.h>
#include <AppStreamQt/release.h>
#include <AppStreamQt/screenshot.h>

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KSycoca>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <glib.h>

namespace
{
// Apps installed while the Store is open are not in KDE's service cache
// yet; reading the launcher file directly finds them anyway.
KService::Ptr findService(const QString &desktopId)
{
    if (desktopId.isEmpty()) {
        return {};
    }
    KSycoca::self()->ensureCacheValid();
    KService::Ptr service = KService::serviceByStorageId(desktopId);
    if (service) {
        return service;
    }
    const QString path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId);
    if (!path.isEmpty()) {
        return KService::Ptr(new KService(path));
    }
    return {};
}

const AppStream::Component *primary(const Record *r, const QString &source)
{
    if (!r) {
        return nullptr;
    }
    if (source == QLatin1String("flatpak") && r->flat) {
        return &*r->flat;
    }
    if (r->apt) {
        return &*r->apt;
    }
    return r->flat ? &*r->flat : nullptr;
}

QString themeIcon(const QString &name)
{
    return QStringLiteral("image://theme/") + QString::fromUtf8(QUrl::toPercentEncoding(name));
}

QStringList keyList(GKeyFile *file, const char *key)
{
    QStringList out;
    gsize length = 0;
    gchar **values = g_key_file_get_string_list(file, "Context", key, &length, nullptr);
    for (gsize i = 0; values && i < length; ++i) {
        const QString v = QString::fromUtf8(values[i]).trimmed();
        if (!v.isEmpty()) {
            out << v;
        }
    }
    g_strfreev(values);
    return out;
}

// "xdg-pictures:ro" and "!xdg-pictures" both concern xdg-pictures.
QString baseOf(QString entry)
{
    if (entry.startsWith(QLatin1Char('!'))) {
        entry.remove(0, 1);
    }
    const int colon = entry.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        entry.truncate(colon);
    }
    if (entry.endsWith(QLatin1Char('/'))) {
        entry.chop(1);
    }
    return entry;
}

// The state an entry list gives a permission: 1 granted, 0 denied, -1 silent.
int stateIn(const QStringList &entries, const QString &base)
{
    int result = -1;
    for (const QString &e : entries) {
        if (baseOf(e) == base) {
            result = e.startsWith(QLatin1Char('!')) ? 0 : 1;
        }
    }
    return result;
}

struct PermissionDef {
    const char *group;
    const char *key;
    const char *base;
    const char *label;
    bool always;
};

const QList<PermissionDef> &permissionDefs()
{
    static const QList<PermissionDef> defs = {
        {"filesystems", "fs", "host", "Every file on this computer", false},
        {"filesystems", "fs", "home", "Your home folder", false},
        {"filesystems", "fs", "xdg-documents", "Your Documents folder", false},
        {"filesystems", "fs", "xdg-pictures", "Your Pictures folder", false},
        {"filesystems", "fs", "xdg-music", "Your Music folder", false},
        {"filesystems", "fs", "xdg-videos", "Your Videos folder", false},
        {"filesystems", "fs", "xdg-download", "Your Downloads folder", false},
        {"filesystems", "fs", "xdg-desktop", "Your Desktop", false},
        {"sockets", "sockets", "cups", "Printer", false},
        {"shared", "shared", "network", "Internet", true},
        {"devices", "devices", "all", "Camera and other devices", false},
        {"sockets", "sockets", "pulseaudio", "Sound and microphone", false},
    };
    return defs;
}
}

StoreApp::StoreApp(Catalog *catalog, const QString &key)
    : QObject(catalog)
    , m_catalog(catalog)
    , m_key(key)
{
}

bool StoreApp::isValid() const
{
    return m_catalog->record(m_key) != nullptr;
}

QString StoreApp::appstreamId() const
{
    const Record *r = m_catalog->record(m_key);
    const auto c = primary(r, source());
    return c ? c->id() : m_key;
}

QString StoreApp::serviceName() const
{
    if (!isInstalled()) {
        return {};
    }
    KService::Ptr service = findService(desktopId());
    return service ? service->name() : QString();
}

QString StoreApp::name() const
{
    // Wild OS gives some apps plainer names than their makers do; once
    // installed, the Store calls them what Start does.
    const QString fromStart = serviceName();
    if (!fromStart.isEmpty()) {
        return fromStart;
    }
    const Record *r = m_catalog->record(m_key);
    if (r && r->curated && !r->curated->name.isEmpty()) {
        return r->curated->name;
    }
    const auto c = primary(r, source());
    return c ? c->name() : m_key;
}

QString StoreApp::summary() const
{
    const Record *r = m_catalog->record(m_key);
    if (r && r->curated && !r->curated->summary.isEmpty()) {
        return r->curated->summary;
    }
    const auto c = primary(r, source());
    return c ? c->summary() : QString();
}

QString StoreApp::tagline() const
{
    const Record *r = m_catalog->record(m_key);
    if (r && r->curated && !r->curated->tagline.isEmpty()) {
        return r->curated->tagline;
    }
    return summary();
}

QString StoreApp::developer() const
{
    const Record *r = m_catalog->record(m_key);
    const auto c = primary(r, source());
    QString name = c ? c->developer().name() : QString();
    if (name.isEmpty() && r && r->flat) {
        name = r->flat->developer().name();
    }
    // Desktop projects credit themselves on their own apps ("KDE", "The
    // GNOME Project"); to someone from Windows that names a stranger, not a
    // maker, so the Store leaves it out and the app reads as open source.
    static const QStringList projects = {QStringLiteral("KDE"), QStringLiteral("The KDE Community"), QStringLiteral("KDE e.V."),
                                         QStringLiteral("GNOME"), QStringLiteral("The GNOME Project"), QStringLiteral("GNOME Foundation")};
    if (projects.contains(name.trimmed(), Qt::CaseInsensitive)) {
        return QString();
    }
    return name;
}

QString StoreApp::byline() const
{
    const QString who = developer();
    if (who.isEmpty()) {
        return tagline();
    }
    return tagline() + QStringLiteral(", by ") + who;
}

QString StoreApp::description() const
{
    const Record *r = m_catalog->record(m_key);
    const auto c = primary(r, source());
    QString text = c ? c->description() : QString();
    if (text.trimmed().isEmpty() && r && r->curated) {
        text = QStringLiteral("<p>") + r->curated->description.toHtmlEscaped() + QStringLiteral("</p>");
    }
    // AppStream markup is a small subset of HTML; Qt's rich text reads it,
    // but needs the list items tightened and inline code kept plain.
    text.replace(QRegularExpression(QStringLiteral("\\s*\\n\\s*")), QStringLiteral(" "));
    text.replace(QStringLiteral("<code>"), QString()).replace(QStringLiteral("</code>"), QString());
    // Flathub packagers often add a note about the package itself ("This
    // flatpak is community maintained..."), which means nothing to people
    // installing an app; drop those paragraphs.
    static const QRegularExpression packagingNote(QStringLiteral("<p>(?:(?!</p>).)*(?:flatpak|flathub)(?:(?!</p>).)*</p>"),
                                                  QRegularExpression::CaseInsensitiveOption);
    QString cleaned = text;
    cleaned.remove(packagingNote);
    if (!cleaned.trimmed().isEmpty()) {
        text = cleaned;
    }
    // Qt gives each paragraph a full line of space above and below; the
    // design runs them closer.
    text.replace(QStringLiteral("<p>"), QStringLiteral("<p style=\"margin-top:0; margin-bottom:10px\">"));
    text.replace(QStringLiteral("<ul>"), QStringLiteral("<ul style=\"margin-top:0; margin-bottom:10px\">"));
    return text.trimmed();
}

QString StoreApp::iconSource() const
{
    const Record *r = m_catalog->record(m_key);
    if (!r) {
        return themeIcon(QStringLiteral("application-x-executable"));
    }

    if (isInstalled()) {
        KService::Ptr service = findService(desktopId());
        if (service && !service->icon().isEmpty()) {
            const QString icon = service->icon();
            if (icon.startsWith(QLatin1Char('/'))) {
                return QUrl::fromLocalFile(icon).toString();
            }
            if (QIcon::hasThemeIcon(icon)) {
                return themeIcon(icon);
            }
        }
    }

    const auto c = primary(r, source());
    // Flathub's catalogue arrives a few seconds after the first launch.
    // Until then a curated app has a name but nothing to draw, and an empty
    // tile reads better than a broken file icon.
    if (!c && !(r->curated && !r->curated->icon.isEmpty())) {
        return QString();
    }
    const QList<AppStream::Icon> icons = c ? c->icons() : QList<AppStream::Icon>();

    QString best;
    uint bestSize = 0;
    QString remote;
    const QString flatDir = m_catalog->flatpakAppstreamDir();
    for (const AppStream::Icon &icon : icons) {
        if (icon.kind() == AppStream::Icon::KindCached || icon.kind() == AppStream::Icon::KindLocal) {
            QString path = icon.url().isLocalFile() ? icon.url().toLocalFile() : icon.url().toString();
            if (!QFileInfo::exists(path) && c == (r->flat ? &*r->flat : nullptr) && !flatDir.isEmpty()) {
                path = QStringLiteral("%1/icons/%2x%3/%4").arg(flatDir).arg(icon.width()).arg(icon.height()).arg(icon.name());
            }
            if (QFileInfo::exists(path) && icon.width() > bestSize) {
                best = path;
                bestSize = icon.width();
            }
        } else if (icon.kind() == AppStream::Icon::KindRemote && remote.isEmpty()) {
            remote = icon.url().toString();
        }
    }
    // Catalogue icons on disk first: the remote copies of older catalogue
    // entries are not always kept online.
    if (!best.isEmpty()) {
        return QUrl::fromLocalFile(best).toString();
    }
    if (!remote.isEmpty()) {
        return remote;
    }
    if (r->curated && !r->curated->icon.isEmpty()) {
        return themeIcon(r->curated->icon);
    }
    for (const AppStream::Icon &icon : icons) {
        if (icon.kind() == AppStream::Icon::KindStock) {
            return themeIcon(icon.name());
        }
    }
    return themeIcon(QStringLiteral("application-x-executable"));
}

QVariantList StoreApp::screenshots() const
{
    const Record *r = m_catalog->record(m_key);
    // Flathub keeps every screenshot online; the Wild OS catalogue's links
    // can point at media that has since been pruned.
    auto c = primary(r, source());
    if (r && r->flat && !r->flat->screenshotsAll().isEmpty()) {
        c = &*r->flat;
    }
    QVariantList out;
    if (!c) {
        return out;
    }
    for (const AppStream::Screenshot &shot : c->screenshotsAll()) {
        if (shot.mediaKind() != AppStream::Screenshot::MediaKindImage) {
            continue;
        }
        const QList<AppStream::Image> images = shot.images();
        if (images.isEmpty()) {
            continue;
        }
        AppStream::Image thumb = images.constFirst();
        AppStream::Image full = images.constFirst();
        for (const AppStream::Image &img : images) {
            if (qAbs(int(img.width()) - 752) < qAbs(int(thumb.width()) - 752)) {
                thumb = img;
            }
            if (img.kind() == AppStream::Image::KindSource || img.width() > full.width()) {
                full = img;
            }
        }
        out << QVariantMap{
            {QStringLiteral("thumb"), thumb.url().toString()},
            {QStringLiteral("full"), full.url().toString()},
            {QStringLiteral("width"), thumb.width()},
            {QStringLiteral("height"), thumb.height()},
            {QStringLiteral("caption"), shot.caption()},
        };
        if (out.size() >= 8) {
            break;
        }
    }
    return out;
}

QString StoreApp::categoryLabel() const
{
    const Record *r = m_catalog->record(m_key);
    if (!r) {
        return {};
    }
    if (r->categories.contains(QStringLiteral("Game"))) {
        return QStringLiteral("Games");
    }
    if (r->curated && !r->curated->category.isEmpty()) {
        return m_catalog->categoryLabel(r->curated->category);
    }
    static const QList<QPair<QString, QString>> order = {
        {QStringLiteral("Office"), QStringLiteral("office")},
        {QStringLiteral("Graphics"), QStringLiteral("graphics")},
        {QStringLiteral("AudioVideo"), QStringLiteral("media")},
        {QStringLiteral("Network"), QStringLiteral("internet")},
        {QStringLiteral("Development"), QStringLiteral("development")},
        {QStringLiteral("Education"), QStringLiteral("education")},
        {QStringLiteral("Science"), QStringLiteral("education")},
        {QStringLiteral("System"), QStringLiteral("system")},
        {QStringLiteral("Utility"), QStringLiteral("utilities")},
    };
    for (const auto &pair : order) {
        if (r->categories.contains(pair.first)) {
            return m_catalog->categoryLabel(pair.second);
        }
    }
    return m_catalog->categoryLabel(QStringLiteral("utilities"));
}

bool StoreApp::openSource() const
{
    const auto c = primary(m_catalog->record(m_key), source());
    return c && c->isFloss();
}

QString StoreApp::licence() const
{
    const auto c = primary(m_catalog->record(m_key), source());
    return c ? c->projectLicense() : QString();
}

QString StoreApp::homepage() const
{
    const auto c = primary(m_catalog->record(m_key), source());
    return c ? c->url(AppStream::Component::UrlKindHomepage).toString() : QString();
}

#define CURATED_FIELD(fn, field)                                    \
    QString StoreApp::fn() const                                    \
    {                                                               \
        const Record *r = m_catalog->record(m_key);                 \
        return r && r->curated ? r->curated->field : QString();     \
    }

CURATED_FIELD(insteadOf, insteadOf)
CURATED_FIELD(cardNote, cardNote)
CURATED_FIELD(chip, chip)
CURATED_FIELD(swapTitle, swapTitle)
CURATED_FIELD(swapText, swapText)

bool StoreApp::hasFlatpak() const
{
    const Record *r = m_catalog->record(m_key);
    return r && (r->flat.has_value() || (!r->flatpakId.isEmpty() && m_catalog->flathubAvailable()));
}

bool StoreApp::hasApt() const
{
    const Record *r = m_catalog->record(m_key);
    return r && (r->apt.has_value() || !r->packages.isEmpty()) && !r->packages.isEmpty();
}

QString StoreApp::source() const
{
    return m_catalog->source(m_key);
}

bool StoreApp::isInstalled() const
{
    return m_catalog->isInstalled(m_key);
}

bool StoreApp::updateAvailable() const
{
    return m_catalog->hasUpdate(m_key);
}

bool StoreApp::isBusy() const
{
    return m_catalog->state(m_key).busy;
}

bool StoreApp::isQueued() const
{
    return m_catalog->state(m_key).queued;
}

int StoreApp::progress() const
{
    return m_catalog->state(m_key).progress;
}

QString StoreApp::phase() const
{
    return m_catalog->state(m_key).phase;
}

QString StoreApp::action() const
{
    return m_catalog->state(m_key).action;
}

QString StoreApp::error() const
{
    return m_catalog->state(m_key).error;
}

QString StoreApp::version() const
{
    const QString installed = m_catalog->installedVersion(m_key);
    if (!installed.isEmpty()) {
        return installed.section(QLatin1Char(':'), -1).section(QLatin1Char('-'), 0, 0).section(QLatin1Char('+'), 0, 0);
    }
    const Record *r = m_catalog->record(m_key);
    const auto c = primary(r, source());
    if (c) {
        const auto releases = c->releasesPlain().entries();
        if (!releases.isEmpty()) {
            return releases.constFirst().version();
        }
    }
    const QString candidate = m_catalog->state(m_key).candidateVersion;
    return candidate.section(QLatin1Char(':'), -1).section(QLatin1Char('-'), 0, 0).section(QLatin1Char('+'), 0, 0);
}

QString StoreApp::sizeText() const
{
    const QLocale locale;
    if (isInstalled()) {
        const qint64 size = m_catalog->installedSize(m_key);
        return size > 0 ? QStringLiteral("Uses %1").arg(locale.formattedDataSize(size, 0, QLocale::DataSizeTraditionalFormat)) : QString();
    }
    // The download size comes from PackageKit, so it only describes the Wild
    // OS package.
    const qint64 download = m_catalog->state(m_key).downloadSize;
    if (download > 0 && source() == QLatin1String("apt")) {
        return QStringLiteral("About %1 to download").arg(locale.formattedDataSize(download, 0, QLocale::DataSizeTraditionalFormat));
    }
    return {};
}

QString StoreApp::desktopId() const
{
    const Record *r = m_catalog->record(m_key);
    if (!r) {
        return {};
    }
    const QString from = m_catalog->installedSource(m_key);
    if (from == QLatin1String("flatpak") || (from.isEmpty() && source() == QLatin1String("flatpak"))) {
        return r->flatpakId + QStringLiteral(".desktop");
    }
    if (r->curated && !r->curated->desktop.isEmpty()) {
        return r->curated->desktop + QStringLiteral(".desktop");
    }
    if (r->apt) {
        const QStringList entries = r->apt->launchable(AppStream::Launchable::KindDesktopId).entries();
        if (!entries.isEmpty()) {
            return entries.constFirst();
        }
        const QString id = r->apt->id();
        return id.endsWith(QLatin1String(".desktop")) ? id : id + QStringLiteral(".desktop");
    }
    return {};
}

bool StoreApp::canOpen() const
{
    if (!isInstalled()) {
        return false;
    }
    if (m_catalog->installedSource(m_key) == QLatin1String("flatpak")) {
        return true;
    }
    return findService(desktopId()) != nullptr;
}

bool StoreApp::inStart() const
{
    if (!isInstalled()) {
        return false;
    }
    KService::Ptr service = findService(desktopId());
    if (!service) {
        // Flathub launchers are exported to a folder Start reads.
        return m_catalog->installedSource(m_key) == QLatin1String("flatpak");
    }
    return !service->noDisplay();
}

QString StoreApp::flatpakOverridePath() const
{
    const Record *r = m_catalog->record(m_key);
    if (!r || r->flatpakId.isEmpty()) {
        return {};
    }
    return FlatpakBackend::userDataDir() + QStringLiteral("/overrides/") + r->flatpakId;
}

QVariantList StoreApp::permissions() const
{
    QVariantList out;
    if (source() != QLatin1String("flatpak")) {
        return out;
    }
    const QString metadata = m_catalog->state(m_key).flatpakMetadata;
    if (metadata.isEmpty()) {
        return out;
    }
    GKeyFile *meta = g_key_file_new();
    const QByteArray data = metadata.toUtf8();
    g_key_file_load_from_data(meta, data.constData(), data.size(), G_KEY_FILE_NONE, nullptr);
    GKeyFile *over = g_key_file_new();
    g_key_file_load_from_file(over, QFile::encodeName(flatpakOverridePath()).constData(), G_KEY_FILE_NONE, nullptr);

    const bool editable = isInstalled();
    bool homeShown = false;
    for (const PermissionDef &d : permissionDefs()) {
        const QString base = QString::fromLatin1(d.base);
        const int def = stateIn(keyList(meta, d.group), base);
        const int user = stateIn(keyList(over, d.group), base);
        const bool requested = def == 1 || user == 1;
        if (!requested && !d.always) {
            continue;
        }
        // Folders inside home add nothing when the whole home is reachable.
        if (homeShown && qstrcmp(d.key, "fs") == 0 && base.startsWith(QLatin1String("xdg-"))) {
            continue;
        }
        if (qstrcmp(d.key, "fs") == 0 && (base == QLatin1String("home") || base == QLatin1String("host")) && requested) {
            homeShown = true;
        }
        const bool allowed = user >= 0 ? user == 1 : def == 1;
        out << QVariantMap{
            {QStringLiteral("token"), QString::fromLatin1(d.group) + QLatin1Char(':') + base},
            {QStringLiteral("label"), QString::fromLatin1(d.label)},
            {QStringLiteral("allowed"), allowed},
            {QStringLiteral("editable"), editable},
            {QStringLiteral("fixed"), false},
        };
        // The file picker is how sandboxed apps open anything else, and it
        // cannot be switched off; it sits after the folders, as designed.
        if (qstrcmp(d.key, "fs") == 0 && base == QLatin1String("xdg-desktop")) {
            out << QVariantMap{
                {QStringLiteral("token"), QString()},
                {QStringLiteral("label"), QStringLiteral("Files you open with it")},
                {QStringLiteral("allowed"), true},
                {QStringLiteral("editable"), false},
                {QStringLiteral("fixed"), true},
            };
        }
    }
    bool hasPicker = false;
    for (const QVariant &v : std::as_const(out)) {
        hasPicker = hasPicker || v.toMap().value(QStringLiteral("fixed")).toBool();
    }
    if (!hasPicker) {
        int at = 0;
        while (at < out.size() && out.at(at).toMap().value(QStringLiteral("token")).toString().startsWith(QLatin1String("filesystems:"))) {
            ++at;
        }
        out.insert(at, QVariantMap{
                           {QStringLiteral("token"), QString()},
                           {QStringLiteral("label"), QStringLiteral("Files you open with it")},
                           {QStringLiteral("allowed"), true},
                           {QStringLiteral("editable"), false},
                           {QStringLiteral("fixed"), true},
                       });
    }

    g_key_file_free(meta);
    g_key_file_free(over);
    return out;
}

bool StoreApp::permissionsUnknown() const
{
    return m_catalog->state(m_key).metadataFailed;
}

void StoreApp::setPermission(const QString &token, bool allowed)
{
    const QString group = token.section(QLatin1Char(':'), 0, 0);
    const QString base = token.section(QLatin1Char(':'), 1);
    const QString path = flatpakOverridePath();
    if (group.isEmpty() || base.isEmpty() || path.isEmpty()) {
        return;
    }
    const QByteArray groupName = group.toUtf8();

    GKeyFile *meta = g_key_file_new();
    const QByteArray data = m_catalog->state(m_key).flatpakMetadata.toUtf8();
    g_key_file_load_from_data(meta, data.constData(), data.size(), G_KEY_FILE_NONE, nullptr);
    const bool byDefault = stateIn(keyList(meta, groupName.constData()), base) == 1;
    g_key_file_free(meta);

    GKeyFile *over = g_key_file_new();
    const QByteArray file = QFile::encodeName(path);
    g_key_file_load_from_file(over, file.constData(), GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS), nullptr);
    QStringList entries = keyList(over, groupName.constData());
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&base](const QString &e) {
                      return baseOf(e) == base;
                  }),
                  entries.end());
    // Only differences from what the app asked for are written, the same
    // way "flatpak override" does it.
    if (allowed != byDefault) {
        entries << (allowed ? base : QStringLiteral("!") + base);
    }
    if (entries.isEmpty()) {
        g_key_file_remove_key(over, "Context", groupName.constData(), nullptr);
    } else {
        QList<QByteArray> raw;
        for (const QString &e : std::as_const(entries)) {
            raw << e.toUtf8();
        }
        QList<const char *> ptrs;
        for (const QByteArray &b : std::as_const(raw)) {
            ptrs << b.constData();
        }
        g_key_file_set_string_list(over, "Context", groupName.constData(), ptrs.constData(), gsize(ptrs.size()));
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    g_key_file_save_to_file(over, file.constData(), nullptr);
    g_key_file_free(over);
    Q_EMIT stateChanged();
}

void StoreApp::install()
{
    m_catalog->install(m_key);
}

void StoreApp::remove()
{
    m_catalog->remove(m_key);
}

void StoreApp::update()
{
    m_catalog->update(m_key);
}

bool StoreApp::open()
{
    KService::Ptr service = findService(desktopId());
    if (service) {
        auto job = new KIO::ApplicationLauncherJob(service);
        job->start();
        return true;
    }
    const Record *r = m_catalog->record(m_key);
    if (r && m_catalog->installedSource(m_key) == QLatin1String("flatpak")) {
        return QProcess::startDetached(QStringLiteral("flatpak"), {QStringLiteral("run"), r->flatpakId});
    }
    return false;
}

void StoreApp::useSource(const QString &source)
{
    if (isInstalled()) {
        return;
    }
    m_catalog->state(m_key).chosenSource = source;
    Q_EMIT dataChanged();
    Q_EMIT stateChanged();
    loadDetails();
}

void StoreApp::loadDetails()
{
    m_catalog->fetchDetails(m_key);
}

void StoreApp::clearError()
{
    m_catalog->state(m_key).error.clear();
    Q_EMIT stateChanged();
}
