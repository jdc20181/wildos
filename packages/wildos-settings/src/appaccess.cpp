#include "appaccess.h"

#include <KDesktopFile>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <glib.h>

namespace
{
const QString portalService = QStringLiteral("org.freedesktop.impl.portal.PermissionStore");
const QString portalPath = QStringLiteral("/org/freedesktop/impl/portal/PermissionStore");
const char *geoclue = "org.freedesktop.GeoClue2";

QString userData()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

QStringList installs()
{
    return {QStringLiteral("/var/lib/flatpak"), userData() + QStringLiteral("/flatpak")};
}

// The next three are the Store's own (wildos-store storeapp.cpp), so a
// switch here and a switch there read and write an override the same way.
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

// 1 granted, 0 denied, -1 not mentioned.
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

struct Folder {
    const char *base;
    const char *label;
};

// The Store's labels for the same folders, in its order.
const QList<Folder> &folders()
{
    static const QList<Folder> list = {
        {"host", "Every file on this computer"},
        {"home", "Your home folder"},
        {"xdg-documents", "Your Documents folder"},
        {"xdg-pictures", "Your Pictures folder"},
        {"xdg-music", "Your Music folder"},
        {"xdg-videos", "Your Videos folder"},
        {"xdg-download", "Your Downloads folder"},
        {"xdg-desktop", "Your Desktop"},
    };
    return list;
}

GKeyFile *load(const QByteArray &data)
{
    GKeyFile *file = g_key_file_new();
    g_key_file_load_from_data(file, data.constData(), data.size(), G_KEY_FILE_NONE, nullptr);
    return file;
}

GKeyFile *loadFile(const QString &path, GKeyFileFlags flags = G_KEY_FILE_NONE)
{
    GKeyFile *file = g_key_file_new();
    g_key_file_load_from_file(file, QFile::encodeName(path).constData(), flags, nullptr);
    return file;
}

QString busPolicy(GKeyFile *file)
{
    gchar *value = g_key_file_get_string(file, "System Bus Policy", geoclue, nullptr);
    const QString out = QString::fromUtf8(value);
    g_free(value);
    return out;
}
}

AppAccess::AppAccess(QObject *parent)
    : QObject(parent)
{
    reload();
}

QString AppAccess::overridePath(const QString &id) const
{
    return userData() + QStringLiteral("/flatpak/overrides/") + id;
}

void AppAccess::reload()
{
    m_apps.clear();
    QSet<QString> seen;
    for (const QString &root : installs()) {
        const auto ids = QDir(root + QStringLiteral("/app")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &id : ids) {
            QFile metadata(root + QStringLiteral("/app/") + id + QStringLiteral("/current/active/metadata"));
            if (seen.contains(id) || !metadata.open(QIODevice::ReadOnly)) {
                continue;
            }
            seen.insert(id);
            App app{id, id.section(QLatin1Char('.'), -1), id, metadata.readAll()};
            const QString desktop = root + QStringLiteral("/exports/share/applications/") + id + QStringLiteral(".desktop");
            if (QFileInfo::exists(desktop)) {
                const KDesktopFile file(desktop);
                if (!file.readName().isEmpty()) {
                    app.name = file.readName();
                }
                if (!file.readIcon().isEmpty()) {
                    app.icon = file.readIcon();
                }
            } else {
                // Runtimes and extensions have no launcher; only apps count.
                continue;
            }
            m_apps << app;
        }
    }
    std::sort(m_apps.begin(), m_apps.end(), [](const App &a, const App &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    m_camera = portal(QStringLiteral("devices"), QStringLiteral("camera"));
    m_location = portal(QStringLiteral("location"), QStringLiteral("location"));

    QVariantMap summary;
    for (const QString &kind : {QStringLiteral("camera"), QStringLiteral("microphone"), QStringLiteral("location"), QStringLiteral("files"),
                                QStringLiteral("network")}) {
        const QVariantList rows = apps(kind);
        int allowed = 0;
        for (const QVariant &row : rows) {
            allowed += row.toMap().value(QStringLiteral("allowed")).toBool() ? 1 : 0;
        }
        summary.insert(kind, allowed);
        summary.insert(kind + QStringLiteral("Asked"), rows.size());
    }
    m_summary = summary;
    Q_EMIT changed();
}

int AppAccess::state(const App &app, const QString &group, const QString &base) const
{
    const QByteArray key = group.toUtf8();
    GKeyFile *over = loadFile(overridePath(app.id));
    const int user = stateIn(keyList(over, key.constData()), base);
    g_key_file_free(over);
    if (user >= 0) {
        return user;
    }
    GKeyFile *meta = load(app.metadata);
    const int def = stateIn(keyList(meta, key.constData()), base);
    g_key_file_free(meta);
    return def;
}

QVariantMap AppAccess::portal(const QString &table, const QString &id) const
{
    auto message = QDBusMessage::createMethodCall(portalService, portalPath, portalService, QStringLiteral("Lookup"));
    message << table << id;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    QVariantMap out;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return out;
    }
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        QString app;
        QStringList value;
        arg.beginMapEntry();
        arg >> app >> value;
        arg.endMapEntry();
        out.insert(app, value);
    }
    arg.endMap();
    return out;
}

void AppAccess::setPortal(const QString &table, const QString &id, const QString &app, const QStringList &value)
{
    auto message = QDBusMessage::createMethodCall(portalService, portalPath, portalService, QStringLiteral("SetPermission"));
    message << table << true << id << app << value;
    QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
}

QVariantList AppAccess::apps(const QString &kind) const
{
    QVariantList rows;
    for (const App &app : m_apps) {
        QVariantMap row = {{QStringLiteral("id"), app.id}, {QStringLiteral("name"), app.name}, {QStringLiteral("icon"), app.icon}};
        GKeyFile *meta = load(app.metadata);

        if (kind == QLatin1String("files")) {
            GKeyFile *over = loadFile(overridePath(app.id));
            const QStringList metaList = keyList(meta, "filesystems");
            const QStringList overList = keyList(over, "filesystems");
            g_key_file_free(over);
            QVariantList items;
            bool homeShown = false;
            bool any = false;
            for (const Folder &f : folders()) {
                const QString base = QString::fromLatin1(f.base);
                const int def = stateIn(metaList, base);
                const int user = stateIn(overList, base);
                if (def != 1 && user != 1) {
                    continue;
                }
                // Folders inside home add nothing when all of home is there.
                if (homeShown && base.startsWith(QLatin1String("xdg-"))) {
                    continue;
                }
                homeShown = homeShown || base == QLatin1String("home") || base == QLatin1String("host");
                const bool allowed = user >= 0 ? user == 1 : def == 1;
                any = any || allowed;
                items << QVariantMap{{QStringLiteral("token"), QStringLiteral("filesystems:") + base},
                                     {QStringLiteral("label"), QString::fromLatin1(f.label)},
                                     {QStringLiteral("allowed"), allowed}};
            }
            if (!items.isEmpty()) {
                row.insert(QStringLiteral("items"), items);
                row.insert(QStringLiteral("allowed"), any);
                rows << row;
            }
        } else if (kind == QLatin1String("camera")) {
            const bool devices = stateIn(keyList(meta, "devices"), QStringLiteral("all")) == 1 || state(app, QStringLiteral("devices"), QStringLiteral("all")) == 1;
            const bool asked = m_camera.contains(app.id);
            if (devices || asked) {
                const bool sandbox = state(app, QStringLiteral("devices"), QStringLiteral("all")) == 1;
                const bool answer = m_camera.value(app.id).toStringList().value(0) == QLatin1String("yes");
                row.insert(QStringLiteral("allowed"), devices ? sandbox && (!asked || answer) : answer);
                row.insert(QStringLiteral("token"), QStringLiteral("camera"));
                rows << row;
            }
        } else if (kind == QLatin1String("location")) {
            const bool asked = m_location.contains(app.id);
            const bool bus = busPolicy(meta) == QLatin1String("talk");
            if (asked || bus) {
                bool allowed = !asked || m_location.value(app.id).toStringList().value(0) != QLatin1String("NONE");
                if (bus) {
                    GKeyFile *over = loadFile(overridePath(app.id));
                    const QString user = busPolicy(over);
                    g_key_file_free(over);
                    allowed = allowed && (user.isEmpty() || user == QLatin1String("talk"));
                }
                row.insert(QStringLiteral("allowed"), allowed);
                row.insert(QStringLiteral("token"), QStringLiteral("location"));
                rows << row;
            }
        } else {
            const QString group = kind == QLatin1String("microphone") ? QStringLiteral("sockets") : QStringLiteral("shared");
            const QString base = kind == QLatin1String("microphone") ? QStringLiteral("pulseaudio") : QStringLiteral("network");
            const int effective = state(app, group, base);
            // Every app is listed for the internet, as the Store does.
            if (kind == QLatin1String("network") || effective == 1 || stateIn(keyList(meta, group.toUtf8().constData()), base) == 1) {
                row.insert(QStringLiteral("allowed"), effective == 1);
                row.insert(QStringLiteral("token"), group + QLatin1Char(':') + base);
                rows << row;
            }
        }
        g_key_file_free(meta);
    }
    return rows;
}

void AppAccess::set(const QString &id, const QString &token, bool allowed)
{
    auto it = std::find_if(m_apps.cbegin(), m_apps.cend(), [&id](const App &a) {
        return a.id == id;
    });
    if (it == m_apps.cend()) {
        return;
    }
    const App app = *it;
    const QString path = overridePath(id);
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QByteArray file = QFile::encodeName(path);
    GKeyFile *meta = load(app.metadata);
    GKeyFile *over = loadFile(path, GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS));

    auto writeContext = [&](const QString &group, const QString &base) {
        const QByteArray groupName = group.toUtf8();
        const bool byDefault = stateIn(keyList(meta, groupName.constData()), base) == 1;
        QStringList entries = keyList(over, groupName.constData());
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&base](const QString &e) {
                          return baseOf(e) == base;
                      }),
                      entries.end());
        // Only differences from what the app asked for are written, the
        // same way "flatpak override" and the Store do it.
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
    };

    if (token == QLatin1String("camera")) {
        const bool devices = stateIn(keyList(meta, "devices"), QStringLiteral("all")) == 1 || stateIn(keyList(over, "devices"), QStringLiteral("all")) == 1;
        if (devices) {
            writeContext(QStringLiteral("devices"), QStringLiteral("all"));
        }
        if (m_camera.contains(id) || !devices) {
            setPortal(QStringLiteral("devices"), QStringLiteral("camera"), id, {allowed ? QStringLiteral("yes") : QStringLiteral("no")});
        }
    } else if (token == QLatin1String("location")) {
        if (m_location.contains(id)) {
            QStringList value = m_location.value(id).toStringList();
            if (value.isEmpty()) {
                value = {QString(), QStringLiteral("0")};
            }
            value[0] = allowed ? QStringLiteral("EXACT") : QStringLiteral("NONE");
            setPortal(QStringLiteral("location"), QStringLiteral("location"), id, value);
        }
        if (busPolicy(meta) == QLatin1String("talk")) {
            if (allowed) {
                g_key_file_remove_key(over, "System Bus Policy", geoclue, nullptr);
            } else {
                g_key_file_set_string(over, "System Bus Policy", geoclue, "none");
            }
        }
    } else {
        const QString group = token.section(QLatin1Char(':'), 0, 0);
        const QString base = token.section(QLatin1Char(':'), 1);
        if (!group.isEmpty() && !base.isEmpty()) {
            writeContext(group, base);
        }
    }

    g_key_file_save_to_file(over, file.constData(), nullptr);
    g_key_file_free(over);
    g_key_file_free(meta);
    reload();
}
