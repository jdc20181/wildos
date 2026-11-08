#pragma once

#include <KFileItem>
#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QPointF>
#include <QQmlEngine>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class KCoreDirLister;

// What the desktop shows: This computer, the Home folder (named after the
// user), the Recycle Bin, then everything in ~/Desktop, kept in the chosen
// sort order with the three system icons first, as on Windows.
//
// The model also places the icons. Each icon sits in a cell of a grid that
// fills the screen top to bottom, then left to right. Where the user has
// put an icon is remembered in grid units (`positions`, saved by the
// containment), so it survives a change of icon size; icons nobody has
// placed take the first free cell in sort order.
class DesktopModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl folder READ folder CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortChanged)
    Q_PROPERTY(bool autoArrange READ autoArrange WRITE setAutoArrange NOTIFY layoutOptionsChanged)
    Q_PROPERTY(bool alignToGrid READ alignToGrid WRITE setAlignToGrid NOTIFY layoutOptionsChanged)
    Q_PROPERTY(qreal cellWidth READ cellWidth WRITE setCellWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal cellHeight READ cellHeight WRITE setCellHeight NOTIFY geometryChanged)
    Q_PROPERTY(qreal itemWidth READ itemWidth WRITE setItemWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal itemHeight READ itemHeight WRITE setItemHeight NOTIFY geometryChanged)
    Q_PROPERTY(qreal areaWidth READ areaWidth WRITE setAreaWidth NOTIFY geometryChanged)
    Q_PROPERTY(qreal areaHeight READ areaHeight WRITE setAreaHeight NOTIFY geometryChanged)
    Q_PROPERTY(QString positions READ positions WRITE setPositions NOTIFY positionsChanged)
    Q_PROPERTY(bool listing READ listing NOTIFY listingChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        FileNameRole,
        UrlRole,
        IconNameRole,
        KindRole,
        IsDirRole,
        SelectedRole,
        CutRole,
        PreviewRole,
        StampRole,
        PosXRole,
        PosYRole,
        TypeRole,
        DropTargetRole,
    };
    Q_ENUM(Roles)

    explicit DesktopModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QUrl folder() const { return m_folder; }
    int count() const { return int(m_entries.size()); }
    int selectedCount() const { return int(m_selected.size()); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int row);
    QString sortKey() const { return m_sortKey; }
    void setSortKey(const QString &key);
    bool sortDescending() const { return m_sortDescending; }
    void setSortDescending(bool on);
    bool autoArrange() const { return m_autoArrange; }
    void setAutoArrange(bool on);
    bool alignToGrid() const { return m_alignToGrid; }
    void setAlignToGrid(bool on);
    qreal cellWidth() const { return m_cellWidth; }
    void setCellWidth(qreal w);
    qreal cellHeight() const { return m_cellHeight; }
    void setCellHeight(qreal h);
    qreal itemWidth() const { return m_itemWidth; }
    void setItemWidth(qreal w);
    qreal itemHeight() const { return m_itemHeight; }
    void setItemHeight(qreal h);
    qreal areaWidth() const { return m_areaWidth; }
    void setAreaWidth(qreal w);
    qreal areaHeight() const { return m_areaHeight; }
    void setAreaHeight(qreal h);
    QString positions() const;
    void setPositions(const QString &json);
    bool listing() const { return m_listing; }

    Q_INVOKABLE QUrl urlAt(int row) const;
    Q_INVOKABLE QString kindAt(int row) const;
    Q_INVOKABLE QString fileNameAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;
    Q_INVOKABLE int rowForUrl(const QUrl &url) const;
    Q_INVOKABLE bool isSelected(int row) const;
    Q_INVOKABLE QRectF itemRect(int row) const;

    // Files and folders in ~/Desktop that are selected; the system icons
    // are never copied, moved or deleted.
    Q_INVOKABLE QList<QUrl> selectedUrls() const;
    Q_INVOKABLE QList<int> selectedRows() const;
    Q_INVOKABLE bool selectionHasSystemIcons() const;

    // A click: modifiers as Qt.KeyboardModifiers. Ctrl adds or takes away,
    // Shift selects the run from the last clicked icon.
    Q_INVOKABLE void select(int row, int modifiers);
    Q_INVOKABLE void selectOnly(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void beginRubberBand(bool additive);
    Q_INVOKABLE void updateRubberBand(const QRectF &rect);
    Q_INVOKABLE void endRubberBand();

    Q_INVOKABLE int rowAt(qreal x, qreal y) const;
    // The nearest icon in a direction (Qt.Key_Left and so on), for the
    // arrow keys.
    Q_INVOKABLE int neighbour(int row, int key) const;
    // Typing the start of a name jumps to it, as on Windows.
    Q_INVOKABLE int findByPrefix(const QString &prefix, int from) const;

    // Dragging the selection to a new place on the desktop.
    Q_INVOKABLE void moveSelection(int anchorRow, qreal dx, qreal dy);
    // Where new items should land: the point the menu was opened or a drop
    // was made, kept for a few seconds.
    Q_INVOKABLE void setPlaceHint(qreal x, qreal y);
    // Sort by: lines every icon up in the new order.
    Q_INVOKABLE void sortBy(const QString &key);
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void selectionChanged();
    void currentIndexChanged();
    void sortChanged();
    void layoutOptionsChanged();
    void geometryChanged();
    void positionsChanged();
    void listingChanged();
    // A folder or document just made from the New menu, to name.
    void renameRequested(int row);

private:
    struct Entry {
        QString key;
        QString kind;
        KFileItem item;
        QUrl url;
        QString name;
        QString fileName;
        QString iconName;
        QString type;
        bool isDir = false;
        bool preview = false;
        qint64 size = -1;
        QDateTime modified;
        QPointF cell;
    };

    void makeSystemEntries();
    Entry entryFor(const KFileItem &item) const;
    void refreshNames();
    void addItems(const KFileItemList &items);
    void removeItems(const KFileItemList &items);
    void updateItems(const QList<QPair<KFileItem, KFileItem>> &items);
    void resort();
    bool lessThan(const Entry &a, const Entry &b) const;
    void scheduleLayout();
    void layout();
    int rows() const;
    int columns() const;
    QPoint nearestFree(const QPointF &cell, const QSet<quint64> &taken) const;
    QPoint firstFree(const QSet<quint64> &taken) const;
    void setSelection(const QSet<QString> &keys);
    void emitAll(const QList<int> &roles);
    void onCreated(const QUrl &folder, const QList<QUrl> &urls, bool rename);
    void claimPending();
    void savePositions();
    QPointF hintCell() const;

    QUrl m_folder;
    KCoreDirLister *m_lister;
    QList<Entry> m_entries;
    QSet<QString> m_selected;
    QSet<QString> m_bandBase;
    bool m_bandAdditive = false;
    int m_current = -1;
    int m_anchor = -1;
    QString m_sortKey = QStringLiteral("name");
    bool m_sortDescending = false;
    bool m_autoArrange = false;
    bool m_alignToGrid = true;
    qreal m_cellWidth = 100;
    qreal m_cellHeight = 100;
    qreal m_itemWidth = 92;
    qreal m_itemHeight = 86;
    qreal m_areaWidth = 0;
    qreal m_areaHeight = 0;
    QHash<QString, QPointF> m_positions;
    QHash<QUrl, bool> m_pending;
    QPointF m_hint;
    bool m_hintValid = false;
    QTimer m_hintTimer;
    QTimer m_layoutTimer;
    bool m_listing = false;
};
