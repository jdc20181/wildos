#include "catalog.h"

#include "flatpakbackend.h"
#include "packagekitbackend.h"
#include "storeapp.h"

#include <AppStreamQt/bundle.h>
#include <AppStreamQt/developer.h>
#include <AppStreamQt/icon.h>
#include <AppStreamQt/launchable.h>
#include <AppStreamQt/pool.h>
#include <AppStreamQt/screenshot.h>

#include <KService>

#include <QFile>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QTime>

#include <algorithm>

Catalog *Catalog::s_instance = nullptr;

Q_LOGGING_CATEGORY(lcStore, "wildos.store", QtWarningMsg)

namespace
{
struct CategoryDef {
    const char *id;
    const char *label;
    QStringList freedesktop;
};

const QList<CategoryDef> &appCategoryDefs()
{
    static const QList<CategoryDef> defs = {
        {"office", "Office", {QStringLiteral("Office")}},
        {"internet", "Internet", {QStringLiteral("Network")}},
        {"graphics", "Photos and graphics", {QStringLiteral("Graphics")}},
        {"media", "Music and video", {QStringLiteral("AudioVideo"), QStringLiteral("Audio"), QStringLiteral("Video")}},
        {"education", "Learning", {QStringLiteral("Education"), QStringLiteral("Science")}},
        {"development", "Developer tools", {QStringLiteral("Development")}},
        {"utilities", "Utilities", {QStringLiteral("Utility")}},
        {"system", "System", {QStringLiteral("System"), QStringLiteral("Settings")}},
    };
    return defs;
}

const QList<CategoryDef> &gameCategoryDefs()
{
    static const QList<CategoryDef> defs = {
        {"game-action", "Action", {QStringLiteral("ActionGame"), QStringLiteral("Shooter")}},
        {"game-adventure", "Adventure", {QStringLiteral("AdventureGame"), QStringLiteral("RolePlaying")}},
        {"game-arcade", "Arcade", {QStringLiteral("ArcadeGame")}},
        {"game-puzzle", "Puzzles", {QStringLiteral("LogicGame"), QStringLiteral("BlocksGame")}},
        {"game-board", "Board and cards", {QStringLiteral("BoardGame"), QStringLiteral("CardGame")}},
        {"game-strategy", "Strategy", {QStringLiteral("StrategyGame")}},
        {"game-sports", "Sports and racing", {QStringLiteral("SportsGame")}},
        {"game-emulator", "Emulators", {QStringLiteral("Emulator")}},
    };
    return defs;
}

QString normalise(const QString &id)
{
    QString key = id.trimmed().toLower();
    if (key.endsWith(QLatin1String(".desktop"))) {
        key.chop(8);
    }
    return key;
}

QStringList strings(const QJsonValue &value)
{
    QStringList out;
    if (value.isString()) {
        out << value.toString();
    }
    for (const QJsonValue &v : value.toArray()) {
        out << v.toString();
    }
    return out;
}

int qualityOf(const AppStream::Component &c)
{
    int score = 0;
    if (!c.screenshotsAll().isEmpty()) {
        score += 3;
    }
    if (c.description().size() > 160) {
        score += 1;
    }
    if (c.customValue(QStringLiteral("flathub::verification::verified")) == QLatin1String("true")) {
        score += 2;
    }
    if (!c.developer().name().isEmpty()) {
        score += 1;
    }
    return score;
}
}

Catalog::Catalog(QObject *parent)
    : QObject(parent)
    , m_packagekit(new PackageKitBackend(this))
    , m_flatpak(new FlatpakBackend(this))
{
    s_instance = this;
    loadCurated();

    connect(m_packagekit, &PackageKitBackend::installedChanged, this, &Catalog::bump);
    connect(m_packagekit, &PackageKitBackend::updatesChanged, this, &Catalog::bump);
    connect(m_packagekit, &PackageKitBackend::catalogueRefreshed, this, &Catalog::loadOsPool);
    connect(m_packagekit, &PackageKitBackend::refreshFinished, this, &Catalog::checkPartDone);
    connect(m_packagekit, &PackageKitBackend::progress, this, &Catalog::onProgress);
    connect(m_packagekit, &PackageKitBackend::finished, this, &Catalog::onFinished);
    connect(m_packagekit, &PackageKitBackend::detailsReady, this, [this](const QString &key, qint64 download, qint64, const QString &version) {
        AppState &s = m_states[key];
        s.downloadSize = download;
        s.candidateVersion = version;
        stateChangedFor(key);
    });

    connect(m_flatpak, &FlatpakBackend::appstreamChanged, this, &Catalog::loadFlatpakPool);
    connect(m_flatpak, &FlatpakBackend::installedChanged, this, &Catalog::bump);
    connect(m_flatpak, &FlatpakBackend::refreshFinished, this, &Catalog::checkPartDone);
    connect(m_flatpak, &FlatpakBackend::progress, this, &Catalog::onProgress);
    connect(m_flatpak, &FlatpakBackend::finished, this, &Catalog::onFinished);
    connect(m_flatpak, &FlatpakBackend::metadataReady, this, [this](const QString &appId, const QString &metadata) {
        for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
            if (it->flatpakId == appId) {
                AppState &s = m_states[it.key()];
                if (metadata.isEmpty()) {
                    s.metadataFailed = s.flatpakMetadata.isEmpty();
                } else {
                    s.flatpakMetadata = metadata;
                    s.metadataFailed = false;
                }
                stateChangedFor(it.key());
            }
        }
    });

    loadOsPool();
    m_packagekit->start();
    m_flatpak->start();
}

Catalog::~Catalog()
{
    s_instance = nullptr;
}

Catalog *Catalog::instance()
{
    return s_instance;
}

Catalog *Catalog::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance) {
        new Catalog;
    }
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Catalog::loadCurated()
{
    // The installed copy wins so the list can be tuned without a rebuild;
    // the one inside the binary keeps the Store whole if it goes missing.
    QFile file(QStringLiteral(WILDOS_STORE_DATADIR "/curated.json"));
    if (!file.exists()) {
        file.setFileName(QStringLiteral(":/qt/qml/WildOS/Store/data/curated.json"));
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue &v : root.value(QStringLiteral("apps")).toArray()) {
        const QJsonObject o = v.toObject();
        Curated c;
        c.key = o.value(QStringLiteral("key")).toString();
        c.ids = strings(o.value(QStringLiteral("ids")));
        c.packages = strings(o.value(QStringLiteral("packages")));
        c.flatpak = o.value(QStringLiteral("flatpak")).toString();
        c.prefer = o.value(QStringLiteral("prefer")).toString();
        c.name = o.value(QStringLiteral("name")).toString();
        c.summary = o.value(QStringLiteral("summary")).toString();
        c.tagline = o.value(QStringLiteral("tagline")).toString();
        c.insteadOf = o.value(QStringLiteral("insteadOf")).toString();
        c.aliases = strings(o.value(QStringLiteral("aliases")));
        c.cardNote = o.value(QStringLiteral("cardNote")).toString();
        c.chip = o.value(QStringLiteral("chip")).toString();
        c.swapTitle = o.value(QStringLiteral("swapTitle")).toString();
        c.swapText = o.value(QStringLiteral("swapText")).toString();
        c.description = o.value(QStringLiteral("description")).toString();
        c.icon = o.value(QStringLiteral("icon")).toString();
        c.desktop = o.value(QStringLiteral("desktop")).toString();
        c.category = o.value(QStringLiteral("category")).toString();
        m_curated.append(c);
    }
    m_homeSwaps = strings(root.value(QStringLiteral("homeSwaps")));
    m_homeInstalled = strings(root.value(QStringLiteral("homeInstalled")));
    m_essentials = strings(root.value(QStringLiteral("essentials")));
    const QJsonObject featured = root.value(QStringLiteral("featured")).toObject();
    for (auto it = featured.begin(); it != featured.end(); ++it) {
        m_featured.insert(it.key(), strings(it.value()));
    }
    for (const QString &id : strings(root.value(QStringLiteral("hidden")))) {
        m_hidden.insert(normalise(id));
    }
}

void Catalog::loadOsPool()
{
    auto pool = std::make_unique<AppStream::Pool>();
    pool->setLocale(QLocale::system().name());
    // Flathub is read separately so each app knows where it came from.
    pool->setFlags(AppStream::Pool::Flags(AppStream::Pool::FlagLoadOsCatalog | AppStream::Pool::FlagLoadOsMetainfo));
    AppStream::Pool *raw = pool.get();
    connect(raw, &AppStream::Pool::loadFinished, this, [this, raw](bool) {
        qCDebug(lcStore) << "Wild OS catalogue loaded" << raw->lastError();
        if (m_osPool.get() != raw) {
            m_osPool.reset(raw);
        }
        m_osLoaded = true;
        rebuild();
    });
    pool.release()->loadAsync();
}

void Catalog::loadFlatpakPool()
{
    const QString dir = m_flatpak->appstreamDir();
    if (dir.isEmpty()) {
        return;
    }
    auto pool = new AppStream::Pool;
    pool->setLocale(QLocale::system().name());
    pool->setFlags(AppStream::Pool::FlagNone);
    pool->setLoadStdDataLocations(false);
    pool->addExtraDataLocation(dir, AppStream::Metadata::FormatStyleCatalog);
    connect(pool, &AppStream::Pool::loadFinished, this, [this, pool](bool ok) {
        qCDebug(lcStore) << "Flathub catalogue loaded" << ok << pool->lastError();
        m_flatPool.reset(pool);
        if (m_osLoaded) {
            rebuild();
        }
    });
    pool->loadAsync();
}

QString Catalog::flatpakAppstreamDir() const
{
    return m_flatpak->appstreamDir();
}

void Catalog::rebuild()
{
    QHash<QString, Record> records;
    QHash<QString, QString> aliases;

    auto addComponent = [&](const AppStream::Component &c, bool flat) {
        if (c.kind() != AppStream::Component::KindDesktopApp) {
            return;
        }
        const QString key = normalise(c.id());
        if (m_hidden.contains(key)) {
            return;
        }
        Record &r = records[key];
        r.key = key;
        if (flat) {
            const AppStream::Bundle bundle = c.bundle(AppStream::Bundle::KindFlatpak);
            if (bundle.isEmpty()) {
                return;
            }
            r.flat = c;
            r.flatpakRef = bundle.id();
            r.flatpakId = bundle.id().section(QLatin1Char('/'), 1, 1);
        } else {
            // The catalogue and an installed app's own metainfo can both
            // describe it; the catalogue has the package name, so keep it.
            if (r.apt && !r.apt->packageNames().isEmpty() && c.packageNames().isEmpty()) {
                return;
            }
            r.apt = c;
            r.packages = c.packageNames();
        }
        r.quality = std::max(r.quality, qualityOf(c));
        for (const QString &cat : c.categories()) {
            if (!r.categories.contains(cat)) {
                r.categories << cat;
            }
        }
    };

    if (m_osPool) {
        for (const AppStream::Component &c : m_osPool->componentsByKind(AppStream::Component::KindDesktopApp).toList()) {
            addComponent(c, false);
        }
    }
    if (m_flatPool) {
        for (const AppStream::Component &c : m_flatPool->componentsByKind(AppStream::Component::KindDesktopApp).toList()) {
            addComponent(c, true);
        }
    }

    // Some apps carry different IDs in each catalogue (Thunderbird is
    // net.thunderbird.Thunderbird in one and org.mozilla.Thunderbird in the
    // other); the curated entry joins them under its first ID.
    for (const Curated &c : std::as_const(m_curated)) {
        QString home;
        for (const QString &id : c.ids) {
            if (records.contains(normalise(id))) {
                home = normalise(id);
                break;
            }
        }
        if (home.isEmpty()) {
            home = c.ids.isEmpty() ? c.key : normalise(c.ids.constFirst());
        }
        Record &r = records[home];
        r.key = home;
        r.curated = &c;
        for (const QString &id : c.ids) {
            const QString other = normalise(id);
            if (other == home || !records.contains(other)) {
                continue;
            }
            const Record moved = records.take(other);
            if (!r.apt && moved.apt) {
                r.apt = moved.apt;
                r.packages = moved.packages;
            }
            if (!r.flat && moved.flat) {
                r.flat = moved.flat;
                r.flatpakRef = moved.flatpakRef;
                r.flatpakId = moved.flatpakId;
            }
            r.quality = std::max(r.quality, moved.quality);
            for (const QString &cat : moved.categories) {
                if (!r.categories.contains(cat)) {
                    r.categories << cat;
                }
            }
            aliases.insert(other, home);
        }
        if (!c.packages.isEmpty()) {
            r.packages = c.packages;
        }
        if (!c.flatpak.isEmpty() && r.flatpakId.isEmpty()) {
            // Flathub data is not loaded yet; the ref is still known.
            r.flatpakId = c.flatpak;
            r.flatpakRef = QStringLiteral("app/%1/x86_64/stable").arg(c.flatpak);
        }
        r.quality += 10;
        aliases.insert(c.key, home);
        for (const QString &id : c.ids) {
            aliases.insert(normalise(id), home);
        }
    }

    // Records the curated file named but neither catalogue carries, and that
    // have no package to fall back on, cannot be installed or shown.
    for (auto it = records.begin(); it != records.end();) {
        if (!it->apt && !it->flat && it->packages.isEmpty() && it->flatpakId.isEmpty()) {
            it = records.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        for (const QString &pkg : it->packages) {
            if (!aliases.contains(pkg)) {
                aliases.insert(pkg, it.key());
            }
        }
        if (!it->flatpakId.isEmpty()) {
            aliases.insert(it->flatpakId.toLower(), it.key());
        }
    }

    // There are no download counts in either catalogue. An app packaged
    // both by Wild OS and on Flathub is almost always a well known one, so
    // that stands in for popularity.
    for (auto it = records.begin(); it != records.end(); ++it) {
        if (it->apt && it->flat) {
            it->quality += 4;
        }
    }

    QStringList sorted = records.keys();
    std::sort(sorted.begin(), sorted.end(), [&records](const QString &a, const QString &b) {
        const Record &ra = records[a];
        const Record &rb = records[b];
        if (ra.quality != rb.quality) {
            return ra.quality > rb.quality;
        }
        const QString na = ra.curated && !ra.curated->name.isEmpty() ? ra.curated->name : (ra.flat ? ra.flat->name() : ra.apt ? ra.apt->name() : a);
        const QString nb = rb.curated && !rb.curated->name.isEmpty() ? rb.curated->name : (rb.flat ? rb.flat->name() : rb.apt ? rb.apt->name() : b);
        return na.compare(nb, Qt::CaseInsensitive) < 0;
    });

    qCDebug(lcStore) << "apps:" << records.size() << "os:" << (m_osPool ? m_osPool->components().size() : 0)
                    << "flathub:" << (m_flatPool ? m_flatPool->components().size() : 0);
    m_records = std::move(records);
    m_aliases = std::move(aliases);
    m_sorted = std::move(sorted);

    for (auto it = m_apps.begin(); it != m_apps.end();) {
        if (it.value()) {
            Q_EMIT it.value()->dataChanged();
            Q_EMIT it.value()->stateChanged();
            ++it;
        } else {
            it = m_apps.erase(it);
        }
    }

    if (!m_ready) {
        m_ready = true;
        Q_EMIT readyChanged();
    }
    ++m_revision;
    Q_EMIT revisionChanged();
}

void Catalog::bump()
{
    for (auto it = m_apps.cbegin(); it != m_apps.cend(); ++it) {
        if (it.value()) {
            Q_EMIT it.value()->stateChanged();
        }
    }
    ++m_revision;
    Q_EMIT revisionChanged();
}

void Catalog::stateChangedFor(const QString &key)
{
    if (auto a = m_apps.value(key)) {
        Q_EMIT a->stateChanged();
    }
    Q_EMIT appStateChanged(key);
}

const Record *Catalog::record(const QString &key) const
{
    auto it = m_records.constFind(key);
    return it == m_records.cend() ? nullptr : &it.value();
}

QString Catalog::keyFor(const QString &id) const
{
    const QString key = normalise(id);
    if (m_records.contains(key)) {
        return key;
    }
    return m_aliases.value(key);
}

StoreApp *Catalog::app(const QString &key)
{
    if (key.isEmpty()) {
        return nullptr;
    }
    const QString k = keyFor(key).isEmpty() ? key : keyFor(key);
    if (auto existing = m_apps.value(k)) {
        return existing;
    }
    auto a = new StoreApp(this, k);
    QJSEngine::setObjectOwnership(a, QJSEngine::CppOwnership);
    m_apps.insert(k, a);
    return a;
}

QString Catalog::resolve(const QString &query) const
{
    QString q = query.trimmed();
    // appstream://org.kde.krita, appstream:org.kde.krita and the
    // apt-style appstream:krita all name the same app.
    if (q.startsWith(QLatin1String("appstream:"), Qt::CaseInsensitive)) {
        q = q.mid(10);
        while (q.startsWith(QLatin1Char('/'))) {
            q.remove(0, 1);
        }
        q = q.section(QLatin1Char('?'), 0, 0);
    }
    if (q.startsWith(QLatin1String("app/"))) {
        q = q.section(QLatin1Char('/'), 1, 1);
    }
    return keyFor(q);
}

QString Catalog::source(const QString &key) const
{
    const Record *r = record(key);
    if (!r) {
        return {};
    }
    const QString installedFrom = installedSource(key);
    if (!installedFrom.isEmpty()) {
        return installedFrom;
    }
    const QString chosen = m_states.value(key).chosenSource;
    const bool hasApt = r->apt.has_value() || !r->packages.isEmpty();
    const bool hasFlat = r->flat.has_value() || (!r->flatpakId.isEmpty() && flathubAvailable());
    if (chosen == QLatin1String("apt") && hasApt) {
        return chosen;
    }
    if (chosen == QLatin1String("flatpak") && hasFlat) {
        return chosen;
    }
    if (r->curated && r->curated->prefer == QLatin1String("apt") && hasApt) {
        return QStringLiteral("apt");
    }
    // Flathub apps run in a sandbox and are usually newer, so they win when
    // an app is in both catalogues.
    if (hasFlat && r->flat) {
        return QStringLiteral("flatpak");
    }
    if (hasApt) {
        return QStringLiteral("apt");
    }
    return hasFlat ? QStringLiteral("flatpak") : QString();
}

QString Catalog::installedSource(const QString &key) const
{
    const Record *r = record(key);
    if (!r) {
        return {};
    }
    if (!r->flatpakId.isEmpty() && m_flatpak->installed().contains(r->flatpakId)) {
        return QStringLiteral("flatpak");
    }
    if (!r->packages.isEmpty() && m_packagekit->installed().contains(r->packages.constFirst())) {
        return QStringLiteral("apt");
    }
    return {};
}

bool Catalog::isInstalled(const QString &key) const
{
    return !installedSource(key).isEmpty();
}

bool Catalog::hasUpdate(const QString &key) const
{
    const Record *r = record(key);
    if (!r) {
        return false;
    }
    const QString from = installedSource(key);
    if (from == QLatin1String("flatpak")) {
        return m_flatpak->updates().contains(r->flatpakId);
    }
    if (from == QLatin1String("apt")) {
        const QSet<QString> pending = m_packagekit->updates();
        return std::any_of(r->packages.cbegin(), r->packages.cend(), [&pending](const QString &p) {
            return pending.contains(p);
        });
    }
    return false;
}

QString Catalog::installedVersion(const QString &key) const
{
    const Record *r = record(key);
    if (r && installedSource(key) == QLatin1String("apt")) {
        return m_packagekit->installedVersion(r->packages.constFirst());
    }
    return {};
}

qint64 Catalog::installedSize(const QString &key) const
{
    const Record *r = record(key);
    if (r && installedSource(key) == QLatin1String("flatpak")) {
        return m_flatpak->installedSizes().value(r->flatpakId, -1);
    }
    return -1;
}

int Catalog::updateCount() const
{
    return updates().size();
}

bool Catalog::flathubAvailable() const
{
    return !m_flatpak->appstreamDir().isEmpty();
}

QVariantList Catalog::appCategories() const
{
    QVariantList out;
    for (const CategoryDef &d : appCategoryDefs()) {
        out << QVariantMap{{QStringLiteral("id"), QString::fromLatin1(d.id)}, {QStringLiteral("label"), QString::fromLatin1(d.label)}};
    }
    return out;
}

QVariantList Catalog::gameCategories() const
{
    QVariantList out;
    for (const CategoryDef &d : gameCategoryDefs()) {
        out << QVariantMap{{QStringLiteral("id"), QString::fromLatin1(d.id)}, {QStringLiteral("label"), QString::fromLatin1(d.label)}};
    }
    return out;
}

QString Catalog::categoryLabel(const QString &id) const
{
    for (const CategoryDef &d : appCategoryDefs()) {
        if (id == QLatin1String(d.id)) {
            return QString::fromLatin1(d.label);
        }
    }
    for (const CategoryDef &d : gameCategoryDefs()) {
        if (id == QLatin1String(d.id)) {
            return QString::fromLatin1(d.label);
        }
    }
    if (id == QLatin1String("games")) {
        return QStringLiteral("Games");
    }
    return QStringLiteral("All apps");
}

QStringList Catalog::homeSwaps() const
{
    QStringList out;
    for (const QString &k : m_homeSwaps) {
        const QString key = keyFor(k);
        if (!key.isEmpty()) {
            out << key;
        }
    }
    return out;
}

QStringList Catalog::homeInstalled() const
{
    QStringList out;
    for (const QString &k : m_homeInstalled) {
        const QString key = keyFor(k);
        if (!key.isEmpty() && isInstalled(key) && !out.contains(key)) {
            out << key;
        }
        if (out.size() == 4) {
            return out;
        }
    }
    for (const QString &key : installedApps()) {
        if (!out.contains(key)) {
            out << key;
        }
        if (out.size() == 4) {
            break;
        }
    }
    return out;
}

QStringList Catalog::swaps() const
{
    QStringList out;
    for (const Curated &c : m_curated) {
        const QString key = keyFor(c.key);
        if (!c.insteadOf.isEmpty() && !key.isEmpty()) {
            out << key;
        }
    }
    return out;
}

QStringList Catalog::essentials() const
{
    QStringList out;
    for (const QString &k : m_essentials) {
        const QString key = keyFor(k);
        if (!key.isEmpty()) {
            out << key;
        }
    }
    return out;
}

QStringList Catalog::featured(const QString &list) const
{
    QStringList out;
    for (const QString &k : m_featured.value(list)) {
        const QString key = keyFor(k);
        if (!key.isEmpty() && !out.contains(key)) {
            out << key;
        }
    }
    return out;
}

QStringList Catalog::category(const QString &id, int limit) const
{
    QStringList wanted;
    bool games = id.startsWith(QLatin1String("game"));
    for (const CategoryDef &d : appCategoryDefs()) {
        if (id == QLatin1String(d.id)) {
            wanted = d.freedesktop;
        }
    }
    for (const CategoryDef &d : gameCategoryDefs()) {
        if (id == QLatin1String(d.id)) {
            wanted = d.freedesktop;
        }
    }

    QStringList out;
    for (const QString &key : m_sorted) {
        const Record &r = m_records[key];
        const bool isGame = r.categories.contains(QStringLiteral("Game"));
        if (isGame != games) {
            continue;
        }
        if (!wanted.isEmpty()) {
            if (r.curated && !r.curated->category.isEmpty()) {
                if (r.curated->category != id) {
                    continue;
                }
            } else if (!std::any_of(wanted.cbegin(), wanted.cend(), [&r](const QString &c) {
                           return r.categories.contains(c);
                       })) {
                continue;
            }
        }
        out << key;
        if (limit > 0 && out.size() >= limit) {
            break;
        }
    }
    return out;
}

QStringList Catalog::swapMatches(const QString &text) const
{
    const QString q = text.trimmed().toLower();
    QStringList out;
    if (q.size() < 2) {
        return out;
    }
    for (const Curated &c : m_curated) {
        QStringList words = c.aliases;
        words << c.insteadOf;
        const bool hit = std::any_of(words.cbegin(), words.cend(), [&q](const QString &w) {
            const QString lw = w.toLower();
            return !lw.isEmpty() && (lw.contains(q) || q.contains(lw));
        });
        const QString key = keyFor(c.key);
        if (hit && !key.isEmpty() && !c.insteadOf.isEmpty() && !out.contains(key)) {
            out << key;
        }
    }
    return out;
}

QStringList Catalog::search(const QString &text) const
{
    const QString q = text.trimmed();
    if (q.isEmpty()) {
        return {};
    }
    QHash<QString, uint> scores;
    auto collect = [&](AppStream::Pool *pool) {
        if (!pool) {
            return;
        }
        for (const AppStream::Component &c : pool->search(q).toList()) {
            if (c.kind() != AppStream::Component::KindDesktopApp) {
                continue;
            }
            const QString key = keyFor(c.id());
            if (key.isEmpty()) {
                continue;
            }
            scores[key] = std::max(scores.value(key), c.sortScore());
        }
    };
    collect(m_osPool.get());
    collect(m_flatPool.get());

    const QString lq = q.toLower();
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        const Record &r = it.value();
        QString name = r.curated && !r.curated->name.isEmpty() ? r.curated->name : (r.flat ? r.flat->name() : r.apt ? r.apt->name() : QString());
        name = name.toLower();
        uint bonus = 0;
        if (name == lq) {
            bonus = 1000000;
        } else if (name.startsWith(lq)) {
            bonus = 500000;
        }
        if (bonus) {
            scores[it.key()] = scores.value(it.key()) + bonus;
        }
    }

    QStringList out = scores.keys();
    std::sort(out.begin(), out.end(), [&](const QString &a, const QString &b) {
        const uint sa = scores.value(a) + uint(m_records[a].quality) * 10;
        const uint sb = scores.value(b) + uint(m_records[b].quality) * 10;
        if (sa != sb) {
            return sa > sb;
        }
        return a < b;
    });
    // Apps that stand in for the Windows app someone typed come first.
    const QStringList swapsFirst = swapMatches(q);
    for (auto it = swapsFirst.crbegin(); it != swapsFirst.crend(); ++it) {
        out.removeAll(*it);
        out.prepend(*it);
    }
    return out;
}

QStringList Catalog::installedApps() const
{
    QStringList out;
    for (const QString &key : m_sorted) {
        if (isInstalled(key)) {
            out << key;
        }
    }
    std::sort(out.begin(), out.end(), [this](const QString &a, const QString &b) {
        auto na = const_cast<Catalog *>(this)->app(a)->name();
        auto nb = const_cast<Catalog *>(this)->app(b)->name();
        return na.compare(nb, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QStringList Catalog::updates() const
{
    QStringList out;
    for (const QString &key : m_sorted) {
        if (hasUpdate(key)) {
            out << key;
        }
    }
    return out;
}

void Catalog::fetchDetails(const QString &key)
{
    const Record *r = record(key);
    if (!r) {
        return;
    }
    const QString from = source(key);
    if (from == QLatin1String("apt")) {
        m_packagekit->fetchDetails(key, r->packages);
    } else if (from == QLatin1String("flatpak") && !r->flatpakId.isEmpty()) {
        m_flatpak->fetchMetadata(r->flatpakId, r->flatpakRef);
    }
}

void Catalog::install(const QString &key)
{
    const Record *r = record(key);
    if (!r || isInstalled(key) || m_states.value(key).busy || m_states.value(key).queued) {
        return;
    }
    const QString from = source(key);
    if (from == QLatin1String("flatpak")) {
        enqueue(JobKind::Install, from, {key}, {r->flatpakRef});
    } else if (from == QLatin1String("apt")) {
        enqueue(JobKind::Install, from, {key}, r->packages);
    }
}

void Catalog::remove(const QString &key)
{
    const Record *r = record(key);
    if (!r || m_states.value(key).busy || m_states.value(key).queued) {
        return;
    }
    const QString from = installedSource(key);
    if (from == QLatin1String("flatpak")) {
        enqueue(JobKind::Remove, from, {key}, {r->flatpakRef});
    } else if (from == QLatin1String("apt")) {
        enqueue(JobKind::Remove, from, {key}, r->packages);
    }
}

void Catalog::update(const QString &key)
{
    const Record *r = record(key);
    if (!r || !hasUpdate(key) || m_states.value(key).busy || m_states.value(key).queued) {
        return;
    }
    const QString from = installedSource(key);
    if (from == QLatin1String("flatpak")) {
        enqueue(JobKind::Update, from, {key}, {r->flatpakRef});
    } else {
        enqueue(JobKind::Update, from, {key}, r->packages);
    }
}

void Catalog::installEssentials()
{
    for (const QString &key : essentials()) {
        install(key);
    }
}

void Catalog::updateAll()
{
    QStringList aptKeys, aptPackages, flatKeys;
    const QSet<QString> pending = m_packagekit->updates();
    for (const QString &key : updates()) {
        AppState &s = m_states[key];
        if (s.busy || s.queued) {
            continue;
        }
        const Record &r = m_records[key];
        if (installedSource(key) == QLatin1String("flatpak")) {
            flatKeys << key;
        } else {
            aptKeys << key;
            for (const QString &p : r.packages) {
                if (pending.contains(p)) {
                    aptPackages << p;
                }
            }
        }
    }
    if (!flatKeys.isEmpty()) {
        // An empty list updates every Flathub app and runtime together, so
        // shared runtimes are fetched once.
        enqueue(JobKind::Update, QStringLiteral("flatpak"), flatKeys, {});
    }
    if (!aptKeys.isEmpty()) {
        enqueue(JobKind::Update, QStringLiteral("apt"), aptKeys, aptPackages);
    }
}

void Catalog::refresh()
{
    if (m_checking) {
        return;
    }
    m_checking = true;
    m_checkParts = 2;
    m_checkOk = true;
    Q_EMIT checkChanged();
    m_packagekit->refresh();
    m_flatpak->refreshAppstream();
}

void Catalog::checkPartDone(bool ok)
{
    // PackageKit also refreshes by itself when the Store starts; only a
    // check someone asked for is reported.
    if (!m_checking) {
        return;
    }
    m_checkOk = m_checkOk && ok;
    if (--m_checkParts > 0) {
        return;
    }
    m_checking = false;
    m_checkResult = m_checkOk ? QStringLiteral("ok") : QStringLiteral("failed");
    m_checkedAt = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    Q_EMIT checkChanged();
}

void Catalog::openSettings(const QString &page)
{
    QProcess::startDetached(QStringLiteral("wildos-settings"), {QStringLiteral("--page"), page});
}

void Catalog::enqueue(JobKind kind, const QString &source, const QStringList &keys, const QStringList &targets)
{
    Job job{m_nextJob++, kind, source, keys, targets};
    const QString action = kind == JobKind::Install ? QStringLiteral("install") : kind == JobKind::Remove ? QStringLiteral("remove") : QStringLiteral("update");
    for (const QString &key : keys) {
        AppState &s = m_states[key];
        s.queued = true;
        s.action = action;
        s.error.clear();
        s.progress = -1;
        s.phase = QStringLiteral("queued");
        stateChangedFor(key);
    }
    m_jobs.append(job);
    ++m_revision;
    Q_EMIT revisionChanged();
    runNext();
}

void Catalog::runNext()
{
    if (m_running || m_jobs.isEmpty()) {
        return;
    }
    m_running = true;
    const Job &job = m_jobs.constFirst();
    for (const QString &key : job.keys) {
        AppState &s = m_states[key];
        s.queued = false;
        s.busy = true;
        s.phase = QStringLiteral("prepare");
        stateChangedFor(key);
    }
    if (job.source == QLatin1String("flatpak")) {
        switch (job.kind) {
        case JobKind::Install:
            m_flatpak->install(job.id, job.targets.value(0));
            break;
        case JobKind::Remove:
            m_flatpak->uninstall(job.id, job.targets.value(0));
            break;
        case JobKind::Update:
            m_flatpak->update(job.id, job.targets);
            break;
        }
    } else {
        switch (job.kind) {
        case JobKind::Install:
            m_packagekit->install(job.id, job.targets);
            break;
        case JobKind::Remove:
            m_packagekit->remove(job.id, job.targets);
            break;
        case JobKind::Update:
            m_packagekit->update(job.id, job.targets);
            break;
        }
    }
}

void Catalog::onProgress(int job, int percent, const QString &phase)
{
    for (const Job &j : std::as_const(m_jobs)) {
        if (j.id != job) {
            continue;
        }
        for (const QString &key : j.keys) {
            AppState &s = m_states[key];
            s.progress = percent;
            s.phase = phase;
            stateChangedFor(key);
        }
    }
}

void Catalog::onFinished(int job, bool ok, const QString &error)
{
    for (int i = 0; i < m_jobs.size(); ++i) {
        const Job j = m_jobs.at(i);
        if (j.id != job) {
            continue;
        }
        m_jobs.removeAt(i);
        for (const QString &key : j.keys) {
            AppState &s = m_states[key];
            s.busy = false;
            s.queued = false;
            s.progress = -1;
            s.phase.clear();
            s.error = ok ? QString() : error;
            if (ok && j.kind == JobKind::Remove) {
                s.chosenSource.clear();
            }
            stateChangedFor(key);
            if (ok && j.kind == JobKind::Install) {
                Q_EMIT installed(key);
            } else if (!ok) {
                Q_EMIT failed(key, error);
            }
        }
        break;
    }
    m_running = false;
    ++m_revision;
    Q_EMIT revisionChanged();
    runNext();
}
