#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QUrl>

// The sidebar: the Home folders, This computer, plugged-in drives and the
// Recycle Bin, in the order of FilesDetails.dc.html.
class PlacesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconRole,
        UrlRole,
        KindRole,
        UdiRole,
        RemovableRole,
    };

    static PlacesModel *create(QQmlEngine *, QJSEngine *);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The sidebar row that stands for a folder, so it can be highlighted.
    Q_INVOKABLE int rowFor(const QUrl &url) const;
    Q_INVOKABLE void mount(const QString &udi);

private:
    explicit PlacesModel(QObject *parent = nullptr);
    void reload();

    struct Place {
        QString name;
        QString icon;
        QUrl url;
        QString kind;
        QString udi;
        bool removable = false;
    };
    QList<Place> m_places;
};
