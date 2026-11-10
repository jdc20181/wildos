#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTemporaryDir>

#include <memory>

struct LocalInfo;

// A package file someone opened from Files or a download: a .deb, a
// .flatpakref or a .flatpak bundle. It reads what the file says about
// itself, compares that with what is installed, and installs it. A .deb goes
// through PackageKit, which fetches what it depends on from the Wild OS
// repositories and asks for the password; a Flatpak goes through the Store's
// own libflatpak backend, for the signed-in user.
//
// Nothing here vouches for the file. A .deb's fields (its name, its
// maker) are whatever whoever built it typed, so the page shows them as
// what the file says, never as checked.
class LocalPackage : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    // deb, flatpakref or flatpak; empty for a file the Store can't install.
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    // reading, unreadable, blocked, get, update, installed, newer, working,
    // done or failed.
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    // The package name or the Flatpak app ID.
    Q_PROPERTY(QString id READ id NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // Plain text, paragraphs separated by blank lines.
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY changed)
    Q_PROPERTY(QString fileSizeText READ fileSizeText NOTIFY changed)
    Q_PROPERTY(QString installedSizeText READ installedSizeText NOTIFY changed)
    Q_PROPERTY(QString homepage READ homepage NOTIFY changed)
    Q_PROPERTY(QString maker READ maker NOTIFY changed)
    Q_PROPERTY(QString iconSource READ iconSource NOTIFY changed)
    // Where a Flatpak downloads from and takes its updates from, as a host
    // name; empty when it has nowhere (a bundle with no origin).
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    // What a .deb says it needs, by package name.
    Q_PROPERTY(QStringList depends READ depends NOTIFY changed)
    // What installing it would also bring from the Wild OS repositories,
    // once PackageKit has worked that out.
    Q_PROPERTY(QStringList extras READ extras NOTIFY changed)
    // What it would take off the computer to make room, which a .deb that
    // conflicts with something installed does.
    Q_PROPERTY(QStringList removes READ removes NOTIFY changed)
    Q_PROPERTY(bool checkingDepends READ checkingDepends NOTIFY changed)
    // Why it can't be read, installed, or why the install failed.
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    // A .flatpakref that only points at Flathub: the app's own Store page
    // is the better place for it.
    Q_PROPERTY(QString flathubId READ flathubId NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    // auth, prepare, download or install, while working.
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(bool canOpen READ canOpen NOTIFY changed)

public:
    explicit LocalPackage(QObject *parent = nullptr);
    ~LocalPackage() override;

    QString path() const { return m_path; }
    void setPath(const QString &path);

    QString kind() const;
    QString state() const { return m_state; }
    QString fileName() const;
    QString name() const;
    QString id() const;
    QString summary() const;
    QString description() const;
    QString version() const;
    QString installedVersion() const;
    QString fileSizeText() const;
    QString installedSizeText() const;
    QString homepage() const;
    QString maker() const;
    QString iconSource() const;
    QString origin() const;
    QStringList depends() const;
    QStringList extras() const { return m_extras; }
    QStringList removes() const { return m_removes; }
    bool checkingDepends() const { return m_checkingDepends; }
    QString problem() const { return m_problem; }
    QString flathubId() const;
    int progress() const { return m_progress; }
    QString phase() const { return m_phase; }
    bool canOpen() const;

    Q_INVOKABLE void install();
    Q_INVOKABLE bool open();

    // The local path for a command line or D-Bus argument that names a
    // package file (a path or a file: URL), or empty when it names none.
    static QString fileArgument(const QString &argument);

Q_SIGNALS:
    void pathChanged();
    void changed();

private:
    void read();
    void adopt(const LocalInfo &info);
    void checkDepends();
    void installDeb();
    void installFlatpak();
    void finish(bool ok, const QString &error);

    QString m_path;
    QString m_state = QStringLiteral("reading");
    std::unique_ptr<LocalInfo> m_info;
    QStringList m_extras;
    QStringList m_removes;
    bool m_checkingDepends = false;
    QString m_problem;
    int m_progress = -1;
    QString m_phase;
    int m_job = 0;
    int m_generation = 0;
    // Where the icon read from the file is kept while the page is open.
    std::shared_ptr<QTemporaryDir> m_scratch;
};
