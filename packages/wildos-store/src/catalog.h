#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QVariantList>

#include <AppStreamQt/component.h>

#include "storeapp.h"

#include <memory>
#include <optional>

namespace AppStream
{
class Pool;
}

class FlatpakBackend;
class PackageKitBackend;

// Hand-picked details that AppStream does not carry: what an app replaces
// on Windows, the short lines the design shows on cards, and which source to
// prefer when an app is in both catalogues. Read from curated.json.
struct Curated {
    QString key;
    QStringList ids;
    QStringList packages;
    QString flatpak;
    QString prefer;
    QString name;
    QString summary;
    QString tagline;
    QString insteadOf;
    QStringList aliases;
    QString cardNote;
    QString chip;
    QString swapTitle;
    QString swapText;
    QString description;
    QString icon;
    QString desktop;
    QString category;
};

// One app as the Store shows it, joined from the Wild OS (apt) catalogue,
// Flathub and the curated file.
struct Record {
    QString key;
    std::optional<AppStream::Component> apt;
    std::optional<AppStream::Component> flat;
    const Curated *curated = nullptr;
    QStringList packages;
    QString flatpakId;
    QString flatpakRef;
    QStringList categories;
    int quality = 0;
};

struct AppState {
    bool busy = false;
    bool queued = false;
    int progress = -1;
    QString phase;
    QString action;
    QString error;
    QString chosenSource;
    qint64 downloadSize = -1;
    QString candidateVersion;
    QString flatpakMetadata;
    bool metadataFailed = false;
};

class Catalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY revisionChanged)
    Q_PROPERTY(bool flathubAvailable READ flathubAvailable NOTIFY revisionChanged)
    Q_PROPERTY(QVariantList appCategories READ appCategories CONSTANT)
    Q_PROPERTY(QVariantList gameCategories READ gameCategories CONSTANT)
    Q_PROPERTY(bool busy READ isBusy NOTIFY revisionChanged)
    // "Check for updates": running, and how the last one went ("", ok or
    // failed) and when, so the button can say what it did.
    Q_PROPERTY(bool checking READ isChecking NOTIFY checkChanged)
    Q_PROPERTY(QString checkResult READ checkResult NOTIFY checkChanged)
    Q_PROPERTY(QString checkedAt READ checkedAt NOTIFY checkChanged)

public:
    explicit Catalog(QObject *parent = nullptr);
    ~Catalog() override;

    static Catalog *instance();
    static Catalog *create(QQmlEngine *, QJSEngine *);

    bool isReady() const { return m_ready; }
    int revision() const { return m_revision; }
    int updateCount() const;
    bool flathubAvailable() const;
    QVariantList appCategories() const;
    QVariantList gameCategories() const;
    bool isBusy() const { return !m_jobs.isEmpty(); }
    bool isChecking() const { return m_checking; }
    QString checkResult() const { return m_checkResult; }
    QString checkedAt() const { return m_checkedAt; }

    Q_INVOKABLE StoreApp *app(const QString &key);
    Q_INVOKABLE QString resolve(const QString &query) const;
    Q_INVOKABLE QStringList homeSwaps() const;
    Q_INVOKABLE QStringList homeInstalled() const;
    Q_INVOKABLE QStringList swaps() const;
    Q_INVOKABLE QStringList essentials() const;
    Q_INVOKABLE QStringList featured(const QString &list) const;
    Q_INVOKABLE QStringList category(const QString &id, int limit = -1) const;
    Q_INVOKABLE QStringList search(const QString &text) const;
    Q_INVOKABLE QStringList swapMatches(const QString &text) const;
    Q_INVOKABLE QStringList installedApps() const;
    Q_INVOKABLE QStringList updates() const;
    Q_INVOKABLE QString categoryLabel(const QString &id) const;
    Q_INVOKABLE void installEssentials();
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openSettings(const QString &page);

    // Used by StoreApp.
    const Record *record(const QString &key) const;
    AppState &state(const QString &key) { return m_states[key]; }
    QString source(const QString &key) const;
    QString installedSource(const QString &key) const;
    bool isInstalled(const QString &key) const;
    bool hasUpdate(const QString &key) const;
    QString installedVersion(const QString &key) const;
    qint64 installedSize(const QString &key) const;
    QString flatpakAppstreamDir() const;
    void install(const QString &key);
    void remove(const QString &key);
    void update(const QString &key);
    void fetchDetails(const QString &key);
    void stateChangedFor(const QString &key);

Q_SIGNALS:
    void readyChanged();
    void revisionChanged();
    void checkChanged();
    void appStateChanged(const QString &key);
    // A finished install, so the window can offer to open it.
    void installed(const QString &key);
    void failed(const QString &key, const QString &message);

private:
    enum class JobKind { Install, Remove, Update };
    struct Job {
        int id;
        JobKind kind;
        QString source;
        QStringList keys;
        QStringList targets;
    };

    void loadCurated();
    void loadOsPool();
    void loadFlatpakPool();
    void rebuild();
    void bump();
    void enqueue(JobKind kind, const QString &source, const QStringList &keys, const QStringList &targets);
    void runNext();
    void onProgress(int job, int percent, const QString &phase);
    void onFinished(int job, bool ok, const QString &error);
    void checkPartDone(bool ok);
    QString keyFor(const QString &id) const;

    static Catalog *s_instance;

    std::unique_ptr<AppStream::Pool> m_osPool;
    std::unique_ptr<AppStream::Pool> m_flatPool;
    bool m_osLoaded = false;
    bool m_ready = false;
    int m_revision = 0;

    QList<Curated> m_curated;
    QStringList m_homeSwaps;
    QStringList m_homeInstalled;
    QStringList m_essentials;
    QHash<QString, QStringList> m_featured;
    QSet<QString> m_hidden;

    QHash<QString, Record> m_records;
    QHash<QString, QString> m_aliases;
    QStringList m_sorted;

    QHash<QString, AppState> m_states;
    QHash<QString, QPointer<StoreApp>> m_apps;

    QList<Job> m_jobs;
    int m_nextJob = 1;
    bool m_running = false;

    PackageKitBackend *m_packagekit;
    FlatpakBackend *m_flatpak;

    bool m_checking = false;
    int m_checkParts = 0;
    bool m_checkOk = true;
    QString m_checkResult;
    QString m_checkedAt;
};
