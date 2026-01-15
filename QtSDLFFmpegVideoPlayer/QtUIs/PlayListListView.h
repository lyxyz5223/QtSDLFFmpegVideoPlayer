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
    bool playing = false;
    QColor fontColor{ Qt::black };
    friend class PlayListItemListModel;
    friend class PlayListListViewItemDelegate;
    friend QDataStream& operator<<(QDataStream& out, const PlayListItem& item);
    friend QDataStream& operator>>(QDataStream& in, PlayListItem& item);
};

// 元类型声明，注册PlayListItem到Qt元对象系统，并注册类型id
Q_DECLARE_METATYPE(PlayListItem)
// 流操作符重载，实现PlayListItem的序列化和反序列化
inline QDataStream& operator<<(QDataStream& out, const PlayListItem& item) // 序列化
{
    out << item.url;
    out << item.title;
    // 私有成员如下
    // icon成员不进行序列化
    out << item.playing;
    out << item.fontColor;
    return out;
}
inline QDataStream& operator>>(QDataStream& in, PlayListItem& item) // 反序列化
{
    in >> item.url;
    in >> item.title;
    // 私有成员如下
    // icon成员不进行序列化
    in >> item.playing;
    in >> item.fontColor;
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

    void clearCurrentPlayingIndex() {
        setCurrentPlayingIndex(-1); // 将播放状态清除
    }

    void setCurrentPlayingIndex(qsizetype index) {
        if (m_currentPlayingIndex >= 0 && m_currentPlayingIndex < m_items.size())
        {
            m_items[m_currentPlayingIndex].playing = false; // 将旧的播放状态清除
            setData(this->index(m_currentPlayingIndex), QVariant(normalFontColor), Qt::ForegroundRole);
        }
        m_currentPlayingIndex = index;
        // 手动调用setData更新视图，主要是为了触发视图的重绘
        setData(this->index(index), QVariant(playingFontColor), Qt::ForegroundRole);
        if (index >= 0 && index < m_items.size())
            m_items[index].playing = true;
    }

    qsizetype currentPlayingIndex() const {
        return m_currentPlayingIndex;
    }

    // 查找当前播放的索引，如有多个只返回第一个，找不到返回-1，且正常情况下只应存在1/0个播放中的项
    qsizetype findPlayingIndex() const {
        for (qsizetype i = 0; i < m_items.size(); ++i) {
            if (m_items[i].playing)
                return i;
        }
        return -1;
    }

private:
    QList<PlayListItem> m_items;
    qsizetype m_currentPlayingIndex = -1;
    QColor playingFontColor = QColor(238, 180, 180);
    QColor normalFontColor = QColor(0, 0, 0);
    //QColor currentFontColor{ normalFontColor };


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
    // 
    int iconModeColumnWidth = 300;
};

