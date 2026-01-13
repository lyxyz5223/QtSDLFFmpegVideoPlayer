#pragma once
#include <qlistview.h>
#include <qicon.h>
#include <QStyledItemDelegate.h>
#include <QAbstractListModel>


struct PlayListItem
{
    QString url;
    QString title;
    PlayListItem() {}
    //explicit PlayListItem(const QString& url) : url(url) {} // 显式构造
    explicit PlayListItem(const QString& url, const QString& title) : url(url), title(title) {}
    void updateIcon();
    bool isNull() const {
        return url.isEmpty();
    }
    QIcon getIcon() const {
        return icon;
    }
private:
    QIcon icon;
    friend class PlayListItemListModel;
    friend class PlayListListViewItemDelegate;
};

// 元类型声明，注册PlayListItem到Qt元对象系统，并注册类型id
Q_DECLARE_METATYPE(PlayListItem)
// 流操作符重载，实现PlayListItem的序列化和反序列化
inline QDataStream& operator<<(QDataStream& out, const PlayListItem& item) // 序列化
{
    out << item.url;
    out << item.title;
    return out;
}
inline QDataStream& operator>>(QDataStream& in, PlayListItem& item) // 反序列化
{
    in >> item.url;
    in >> item.title;
    return in;
}

class PlayListItemListModel : public QAbstractListModel
{
public:
    explicit PlayListItemListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    ~PlayListItemListModel() {}

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex& idx) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    bool setData(const QModelIndex& index, const PlayListItem& value, int role = Qt::EditRole);
    virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    // 如果拖动到某个列表项上，则Qt会自动调用该函数进行删除当前列表选中项（旧indexes）
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    // 如果拖动到某个空白区域/列表项之间的间隔，则Qt会调用该函数进行移动
    virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) override;
    virtual bool clearItemData(const QModelIndex& index) override {
        setData(index, PlayListItem(), Qt::EditRole);
        return true;
    }
    bool insertItem(int row, const PlayListItem& item);
    bool insertFile(int row, const QString& url);
    bool appendFile(const QString& url) {
        return insertFile(rowCount(), url);
    }
    bool appendItem(const PlayListItem& item) {
        return insertItem(rowCount(), item);
    }
    // Valid items are enabled, selectable, editable, drag enabled and drop enabled.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid())
            return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
        Qt::ItemFlags flags = QAbstractListModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        return flags;
    }
    virtual Qt::DropActions supportedDragActions() const override {
        return Qt::MoveAction;
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction | Qt::TargetMoveAction;
    }
    virtual Qt::DropActions supportedDropActions() const override {
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction | Qt::TargetMoveAction;
    }
    // 拖放数据处理
    virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;
    virtual bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) const override;
    virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) override;
    // 支持的MIME类型
    virtual QStringList mimeTypes() const override;
    void clear() {
        beginResetModel();
        m_items.clear();
        endResetModel();
    }
    const QList<PlayListItem>& playList() const {
        return m_items;
    }
    void setPlayList(const QList<PlayListItem>& items) {
        beginResetModel();
        m_items = items;
        endResetModel();
    }
private:
    QList<PlayListItem> m_items;
    static QStringList m_mimeTypes;
    static QMap<QString, qsizetype> m_mimeTypeIndexMap;
    void encodeData(const QModelIndexList& indexes, QDataStream& stream) const;
    bool decodeData(int row, int column, const QModelIndex& parent, QDataStream& stream);
    // 处理内部拖放
    bool internalDropMimeData(const QMimeData* data, const QString& format, Qt::DropAction action, int row, int column, const QModelIndex& parent);
    // 处理外部文件拖放
    bool externalFileDropMimeData(const QMimeData* data, const QString& format, Qt::DropAction action, int row, int column, const QModelIndex& parent);
};

class PlayListListViewItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    PlayListListViewItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    ~PlayListListViewItemDelegate() {}
private:
    // painting
    virtual void paint(QPainter* painter,
        const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
    //// editing
    //QWidget* createEditor(QWidget* parent,
    //    const QStyleOptionViewItem& option,
    //    const QModelIndex& index) const override;

    //void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    //void setModelData(QWidget* editor,
    //    QAbstractItemModel* model,
    //    const QModelIndex& index) const override;

    //void updateEditorGeometry(QWidget* editor,
    //    const QStyleOptionViewItem& option,
    //    const QModelIndex& index) const override;

    //// editor factory
    //QItemEditorFactory* itemEditorFactory() const;
    //void setItemEditorFactory(QItemEditorFactory* factory);

    //virtual QString displayText(const QVariant& value, const QLocale& locale) const;

private:
    QSize getScrollBarSize(const QWidget* widget, Qt::Orientation orientation) const;

private:
    int paddingSize = 5;
    // 绘制的item与真item之间的内边距
    int paddingInsideSize = 0;
    // 绘制的item圆角半径
    qreal itemRadius = 7;
};

