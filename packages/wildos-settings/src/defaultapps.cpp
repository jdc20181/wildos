#include "defaultapps.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KService>
#include <KSharedConfig>

#include <QCollator>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

namespace
{
struct Category {
    const char *id;
    const char *title;
    const char *description;
    const char *icon;
    // The first type decides the current app and the choices; the choice is
    // written for all of them.
    QStringList types;
    // Only apps that say they are this kind are offered, so a text editor
    // that can open web pages is not listed as a browser.
    QString desktopCategory;
};

const QList<Category> &categoryList()
{
    static const QList<Category> list = {
        {"browser", "Web browser", "Opens web links", "globe",
         {QStringLiteral("x-scheme-handler/https"), QStringLiteral("x-scheme-handler/http"), QStringLiteral("text/html"),
          QStringLiteral("application/xhtml+xml")},
         QStringLiteral("WebBrowser")},
        {"email", "Email", "Opens email addresses", "mail", {QStringLiteral("x-scheme-handler/mailto")}, QString()},
        {"pictures", "Pictures", "Photos and other images", "image",
         {QStringLiteral("image/jpeg"), QStringLiteral("image/png"), QStringLiteral("image/gif"), QStringLiteral("image/webp"),
          QStringLiteral("image/bmp"), QStringLiteral("image/tiff"), QStringLiteral("image/heif")},
         QString()},
        {"music", "Music", "Songs and other audio", "sound",
         {QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"), QStringLiteral("audio/x-vorbis+ogg"), QStringLiteral("audio/mp4"),
          QStringLiteral("audio/x-wav"), QStringLiteral("audio/ogg")},
         QString()},
        {"video", "Videos", "Films and clips", "video",
         {QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"), QStringLiteral("video/webm"), QStringLiteral("video/quicktime"),
          QStringLiteral("video/x-msvideo")},
         QString()},
        {"pdf", "PDF documents", "Opens PDF files", "document", {QStringLiteral("application/pdf")}, QString()},
        {"text", "Text files", "Plain text and notes", "pencil", {QStringLiteral("text/plain")}, QString()},
        {"files", "Files and folders", "Opens folders", "folder", {QStringLiteral("inode/directory")}, QString()},
    };
    return list;
}

const Category *findCategory(const QString &id)
{
    for (const Category &c : categoryList()) {
        if (QLatin1String(c.id) == id) {
            return &c;
        }
    }
    return nullptr;
}
}

DefaultApps::DefaultApps(QObject *parent)
    : QObject(parent)
{
    reload();
}

void DefaultApps::reload()
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QVariantList list;
    for (const Category &c : categoryList()) {
        const QString mime = c.types.constFirst();
        QVariantList apps;
        QSet<QString> seen;
        const KService::List services = KApplicationTrader::queryByMimeType(mime);
        for (const KService::Ptr &service : services) {
            if (service->noDisplay() || seen.contains(service->storageId())) {
                continue;
            }
            if (!c.desktopCategory.isEmpty() && !service->categories().contains(c.desktopCategory)) {
                continue;
            }
            seen.insert(service->storageId());
            apps << QVariantMap{{QStringLiteral("value"), service->storageId()}, {QStringLiteral("text"), service->name()}};
        }
        std::sort(apps.begin(), apps.end(), [&collator](const QVariant &a, const QVariant &b) {
            return collator.compare(a.toMap().value(QStringLiteral("text")).toString(), b.toMap().value(QStringLiteral("text")).toString()) < 0;
        });
        const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
        list << QVariantMap{
            {QStringLiteral("id"), QString::fromLatin1(c.id)},
            {QStringLiteral("title"), QString::fromLatin1(c.title)},
            {QStringLiteral("description"), QString::fromLatin1(c.description)},
            {QStringLiteral("icon"), QString::fromLatin1(c.icon)},
            {QStringLiteral("current"), preferred ? preferred->storageId() : QString()},
            {QStringLiteral("apps"), apps},
        };
    }
    m_categories = list;
    Q_EMIT changed();
}

void DefaultApps::setDefault(const QString &categoryId, const QString &storageId)
{
    const Category *category = findCategory(categoryId);
    const KService::Ptr service = KService::serviceByStorageId(storageId);
    if (!category || !service) {
        return;
    }

    // The account's own mimeapps.list, which outranks the system one. The
    // app also goes first under Added Associations, as Dolphin's "Open
    // with" does, so it stays offered for each type.
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);
    KConfigGroup defaults = config->group(QStringLiteral("Default Applications"));
    KConfigGroup added = config->group(QStringLiteral("Added Associations"));
    for (const QString &mime : category->types) {
        defaults.writeXdgListEntry(mime, {storageId});
        QStringList associated = added.readXdgListEntry(mime);
        associated.removeAll(storageId);
        associated.prepend(storageId);
        added.writeXdgListEntry(mime, associated);
    }
    config->sync();

    // KDE apps open web links through the browser named here, and email
    // through the one named in emaildefaults.
    if (categoryId == QLatin1String("browser")) {
        KSharedConfig::Ptr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        globals->group(QStringLiteral("General")).writeEntry("BrowserApplication", storageId, KConfig::Notify);
        globals->sync();
    } else if (categoryId == QLatin1String("email")) {
        KSharedConfig::Ptr email = KSharedConfig::openConfig(QStringLiteral("emaildefaults"));
        KConfigGroup group = email->group(QStringLiteral("PROFILE_Default"));
        group.writeEntry("EmailClient", service->exec());
        group.writeEntry("TerminalClient", false);
        email->sync();
    }

    // The service database caches associations; rebuilding it makes the
    // new default count everywhere straight away.
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        reload();
    });
    process->start(QStringLiteral("kbuildsycoca6"), {});
}
