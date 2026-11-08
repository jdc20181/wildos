#include "placesmodel.h"

#include "drives.h"
#include "fileops.h"
#include "foldermodel.h"

#include <QDir>
#include <QStandardPaths>

PlacesModel *PlacesModel::create(QQmlEngine *, QJSEngine *)
{
    static PlacesModel *model = new PlacesModel;
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
    connect(Drives::instance(), &Drives::changed, this, &PlacesModel::reload);
    connect(FileOps::instance(), &FileOps::binCountChanged, this, [this] {
        for (int i = 0; i < m_places.size(); ++i) {
            if (m_places.at(i).kind == QStringLiteral("bin")) {
                Q_EMIT dataChanged(index(i), index(i), {IconRole});
            }
        }
    });
}

void PlacesModel::reload()
{
    QList<Place> places;
    auto folder = [&places](const QString &name, const QString &icon, QStandardPaths::StandardLocation location) {
        const QString path = QStandardPaths::writableLocation(location);
        // An XDG folder that points at Home itself (common for Desktop on
        // minimal setups) would be a second Home; leave it out.
        if (path.isEmpty() || (location != QStandardPaths::HomeLocation && QDir::cleanPath(path) == QDir::homePath())) {
            return;
        }
        places.append({name, icon, QUrl::fromLocalFile(path), QStringLiteral("place"), QString(), false});
    };
    folder(QStringLiteral("Home"), QStringLiteral("home"), QStandardPaths::HomeLocation);
    folder(QStringLiteral("Desktop"), QStringLiteral("desktop"), QStandardPaths::DesktopLocation);
    folder(QStringLiteral("Documents"), QStringLiteral("document"), QStandardPaths::DocumentsLocation);
    folder(QStringLiteral("Downloads"), QStringLiteral("download"), QStandardPaths::DownloadLocation);
    folder(QStringLiteral("Pictures"), QStringLiteral("pictures"), QStandardPaths::PicturesLocation);
    folder(QStringLiteral("Music"), QStringLiteral("music"), QStandardPaths::MusicLocation);
    folder(QStringLiteral("Videos"), QStringLiteral("videos"), QStandardPaths::MoviesLocation);
    places.append({QString(), QString(), QUrl(), QStringLiteral("separator"), QString(), false});
    places.append({QStringLiteral("This computer"), QStringLiteral("computer"), FolderModel::computerUrl(), QStringLiteral("computer"), QString(), false});
    const auto drives = Drives::instance()->list();
    for (const Drive &drive : drives) {
        if (drive.system) {
            continue;
        }
        places.append({drive.name, drive.lineIcon, drive.mounted ? QUrl::fromLocalFile(drive.mountPath) : QUrl(), QStringLiteral("drive"), drive.udi, drive.removable});
    }
    places.append({QStringLiteral("Recycle Bin"), QStringLiteral("bin"), FolderModel::binUrl(), QStringLiteral("bin"), QString(), false});

    beginResetModel();
    m_places = places;
    endResetModel();
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_places.size());
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IconRole, "iconName"},
        {UrlRole, "url"},
        {KindRole, "kind"},
        {UdiRole, "udi"},
        {RemovableRole, "removable"},
    };
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_places.size()) {
        return {};
    }
    const Place &p = m_places.at(index.row());
    switch (role) {
    case NameRole:
        return p.name;
    case IconRole:
        if (p.kind == QStringLiteral("bin")) {
            return FileOps::instance()->binCount() > 0 ? QStringLiteral("bin-full") : QStringLiteral("bin");
        }
        return p.icon;
    case UrlRole:
        return p.url;
    case KindRole:
        return p.kind;
    case UdiRole:
        return p.udi;
    case RemovableRole:
        return p.removable;
    }
    return {};
}

int PlacesModel::rowFor(const QUrl &url) const
{
    const QUrl clean = url.adjusted(QUrl::StripTrailingSlash);
    for (int i = 0; i < m_places.size(); ++i) {
        const QUrl place = m_places.at(i).url.adjusted(QUrl::StripTrailingSlash);
        if (!place.isEmpty() && place == clean) {
            return i;
        }
    }
    if (url.scheme() == QStringLiteral("trash")) {
        for (int i = 0; i < m_places.size(); ++i) {
            if (m_places.at(i).kind == QStringLiteral("bin")) {
                return i;
            }
        }
    }
    return -1;
}

void PlacesModel::mount(const QString &udi)
{
    Drives::instance()->mount(udi);
}
