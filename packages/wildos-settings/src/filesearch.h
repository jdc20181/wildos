#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// Searching files, after Windows 11's Privacy and security > Searching
// Windows: Baloo, the file indexer behind search in Start and Files.
//
// The settings are baloofilerc, the same keys KDE's own file search module
// writes (baloo's baloosettings.kcfg): [Basic Settings] Indexing-Enabled,
// [General] folders, exclude folders, only basic indexing and index hidden
// folders. After a change the running indexer is asked to read them again
// (org.kde.baloo / org.kde.baloo.main.updateConfig), and turning indexing
// on or off goes through balooctl6, which also starts or stops it.
class FileSearch : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool contents READ contents WRITE setContents NOTIFY changed)
    Q_PROPERTY(bool hidden READ hidden WRITE setHidden NOTIFY changed)
    Q_PROPERTY(QStringList folders READ folders NOTIFY changed)
    Q_PROPERTY(QStringList excluded READ excluded NOTIFY changed)
    Q_PROPERTY(QString indexSize READ indexSize NOTIFY changed)
    Q_PROPERTY(bool rebuilding READ rebuilding NOTIFY rebuildingChanged)

public:
    explicit FileSearch(QObject *parent = nullptr);

    bool available() const;
    bool enabled() const;
    void setEnabled(bool on);
    bool contents() const;
    void setContents(bool on);
    bool hidden() const;
    void setHidden(bool on);
    QStringList folders() const;
    QStringList excluded() const;
    QString indexSize() const;
    bool rebuilding() const { return m_rebuilding; }

    // Paths or file:// URLs from a folder dialog.
    Q_INVOKABLE void addFolder(const QString &folder);
    Q_INVOKABLE void removeFolder(const QString &folder);
    Q_INVOKABLE void exclude(const QString &folder);
    Q_INVOKABLE void include(const QString &folder);
    // Deletes the index and builds it again from the start.
    Q_INVOKABLE void rebuild();
    Q_INVOKABLE void reload();

    // "~/Documents" for a folder in home, the path otherwise.
    Q_INVOKABLE QString display(const QString &folder) const;

Q_SIGNALS:
    void changed();
    void rebuildingChanged();
    void failed(const QString &reason);

private:
    void writeList(const char *key, const QStringList &list);
    void apply(bool startOrStop);

    bool m_rebuilding = false;
};
