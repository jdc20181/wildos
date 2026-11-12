#pragma once

#include "reader.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QSet>

// The rows of the table: the folder of the archive in view, or what a
// search found. Folders come first, then files in the order the archive
// keeps them, unless a column is clicked to sort by it.
class EntryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum Role {
        IndexRole = Qt::UserRole + 1,
        NameRole,
        IsDirRole,
        SizeRole,
        PackedRole,
        ModifiedRole,
        TypeRole,
        KindRole,
        CheckedRole,
        ThumbRole,
        LocationRole,
    };

    explicit EntryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setListing(const Listing *listing, const QString &thumbKey);
    void show(const QList<int> &entries, bool searching);
    void setSort(const QString &key, bool ascending);
    void thumbnailReady(int entry);

    int entryAt(int row) const;
    int rowOf(int entry) const;
    const QList<int> &rows() const
    {
        return m_rows;
    }
    bool isChecked(int row) const;
    void setChecked(const QSet<int> &entries);
    const QSet<int> &checked() const
    {
        return m_checked;
    }

private:
    void sortRows();

    const Listing *m_listing = nullptr;
    QString m_thumbKey;
    QList<int> m_shown;
    QList<int> m_rows;
    QSet<int> m_checked;
    QSet<int> m_thumbs;
    QString m_sortKey;
    bool m_ascending = true;
    bool m_searching = false;
};
