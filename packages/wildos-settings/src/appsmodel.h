#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>

// The apps someone can see in Start, with where each came from and whether
// removing it is safe. Wild OS's own apps are protected, the same way
// Windows does not offer to uninstall Settings.
class AppsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString busyId READ busyId NOTIFY busyChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconRole,
        IdRole,
        SourceRole,
        PackageRole,
        CanUninstallRole,
    };

    explicit AppsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    QString busyId() const { return m_busyId; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void uninstall(const QString &id);

Q_SIGNALS:
    void filterChanged();
    void countChanged();
    void busyChanged();
    void uninstalled(const QString &name);
    void uninstallFailed(const QString &name, const QString &reason);

private:
    struct App {
        QString name;
        QString icon;
        QString id;
        QString flatpak;
        bool userFlatpak = false;
        QString package;
        bool protectedApp = false;
    };

    void applyFilter();
    void finishUninstall(const App &app, bool ok, const QString &reason);
    QHash<QString, QString> desktopOwners() const;
    QSet<QString> protectedPackages() const;

    QList<App> m_all;
    QList<App> m_shown;
    QString m_filter;
    QString m_busyId;
};
