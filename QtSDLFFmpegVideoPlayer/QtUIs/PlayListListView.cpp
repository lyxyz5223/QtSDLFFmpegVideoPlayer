#include "PlayListListView.h"
#include <QFileDialog>
#include <QFileIconProvider>
#include <QPainter>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QMimeData>
#include <QBitArray>
#include <QMouseEvent>
#include <random>
#include <unordered_set>

#include "PlayerPredefine.h"

#define APP_PLAYLIST_MIMETYPE_INDEX 0
#define FILE_URI_LIST_MIMETYPE_INDEX 1

QStringList PlayListItemListModel::m_mimeTypes
{
    "application/x-playlistitemlistmodeldatalist", // 自定义MIME类型，由appMimeTypeIndex指定索引
    "text/uri-list" // 支持外部文件拖放
};

#define MIMETYPE_INDEX_MAP_PAIR(index) { m_mimeTypes.at(index), index }
QMap<QString, qsizetype> PlayListItemListModel::m_mimeTypeIndexMap
{
    MIMETYPE_INDEX_MAP_PAIR(0),
    MIMETYPE_INDEX_MAP_PAIR(1)
};

void PlayListItem::updateIcon()
{
    QFileIconProvider iconProvider;
    QIcon fileIcon = iconProvider.icon(QFileInfo(url));
    icon = fileIcon;
}

void PlayListItem::updateMediaMetaData()
{
    AVFormatContext* fmtCtx = nullptr;
    bool rst = MediaDecodeUtils::openFile(nullptr, fmtCtx, fileInfo.absoluteFilePath().toStdString());
    if (rst)
    {
        for (const AVDictionaryEntry* lastPair = nullptr, *pair = nullptr;pair = av_dict_iterate(fmtCtx->metadata, lastPair); lastPair = pair)
        {
            if (strcmp(pair->key, "title") == 0)
                this->title = pair->value;
            else if (strcmp(pair->key, "artist") == 0)
                this->artist = pair->value;

        }
        if (MediaDecodeUtils::findStreamInfo(nullptr, fmtCtx))
            this->duration = ((fmtCtx->duration > 0) ? fmtCtx->duration / AV_TIME_BASE : 0);
        MediaDecodeUtils::closeFile(nullptr, fmtCtx);
    }
}

int PlayListItemListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QModelIndex PlayListItemListModel::sibling(int row, int column, const QModelIndex& idx) const
{
    if (!idx.isValid() || column != 0 || row >= m_items.size() || row < 0)
        return QModelIndex();

    return createIndex(row, 0);
}

QVariant PlayListItemListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_items.size())
        return QVariant();
    auto& item = m_items.at(index.row());
    switch (role)
    {
    case Qt::DisplayRole: // 主要显示文本
    case Qt::EditRole: // 编辑时的文本（通常和 DisplayRole 相同）
        return QVariant::fromValue(item);
        return item.fileInfo.fileName();
    case Qt::ToolTipRole: // 鼠标悬停提示
        return tr("标题: %1\n文件名: %2\n来源: %3")
            .arg(item.title)
            .arg(item.fileInfo.fileName())
            .arg(item.url);
    case Qt::StatusTipRole: // 状态栏提示（可选）
        return item.title;
    case Qt::WhatsThisRole: // "这是什么"帮助文本（可选）
        return tr("播放列表项目: ") + item.fileInfo.fileName();
    case Qt::CheckStateRole: // 复选框状态（如果有）
        return QVariant();
    case Qt::ForegroundRole: // 前景色（文本颜色）
        return QVariant(item.fontColor);
    //case Qt::BackgroundRole:
    //    return QVariant(QBrush(QColor(220, 220, 220)));
    //case Qt::BackgroundRole:
    //    return QVariant(QBrush(QColor(255, 255, 255, 0)));
    default:
        break;
    }
    return QVariant();
}

bool PlayListItemListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() < 0)
        return false;
    if (role == Qt::EditRole || role == Qt::DisplayRole)
        return setData(index, value.value<PlayListItem>(), role);
    else if (role == Qt::ForegroundRole)
    {
        if (value.canConvert<QColor>())
        {
            m_items[index.row()].fontColor = value.value<QColor>();
            emit dataChanged(index, index, { Qt::ForegroundRole });
            return true;
        }
    }
    return false;
}

// 条件：(role == Qt::EditRole || role == Qt::DisplayRole)
bool PlayListItemListModel::setData(const QModelIndex& index, const PlayListItem& value, int role)
{
    if (index.isValid() && index.row() < m_items.size() && (role == Qt::EditRole || role == Qt::DisplayRole))
    {
        PlayListItem item = value;
        m_items.replace(index.row(), item);
        emit dataChanged(index, index, { Qt::DisplayRole | Qt::EditRole });
        return true;
    }
    return false;
}

bool PlayListItemListModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (row < 0 || row > m_items.size())
        return false;
    beginInsertRows(QModelIndex(), row, row + count - 1);
    m_items.insert(row, count, PlayListItem());
    // 插入后调整正在播放的列表项索引
    if (row <= m_currentPlayingIndex)
        m_currentPlayingIndex += count;
    endInsertRows();
    return true;
}

bool PlayListItemListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (count <= 0 || row < 0 || (row + count) > rowCount(parent))
        return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    
    if (row <= m_currentPlayingIndex && static_cast<qsizetype>(row) + count > m_currentPlayingIndex)
        m_currentPlayingIndex = -1; // Removed the currently playing item
    const auto it = m_items.begin() + row;
    m_items.erase(it, it + count);
    if (static_cast<qsizetype>(row) + count <= m_currentPlayingIndex)
        m_currentPlayingIndex -= count;
    else if (static_cast<qsizetype>(row) <= m_currentPlayingIndex)
        m_currentPlayingIndex = -1; // Removed the currently playing item
    endRemoveRows();
    return true;
}

// copy from QStringListModel::moveRows
bool PlayListItemListModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild)
{
    if (count <= 0
        || destinationChild < 0
        || sourceRow < 0
        || sourceRow == destinationChild
        || sourceRow == destinationChild - 1
        || sourceParent.isValid()
        || destinationParent.isValid()) {
        return false;
    }

    if (const auto rc = rowCount(); sourceRow + count - 1 >= rc || destinationChild > rc)
        return false;

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild))
        return false;

    // move [sourceRow, count) into destinationChild:
    if (sourceRow < destinationChild) { // moving down
        auto beg = m_items.begin() + sourceRow;
        auto end = beg + count;
        auto to = m_items.begin() + destinationChild;
        std::rotate(beg, end, to);
        if (sourceRow <= m_currentPlayingIndex && static_cast<qsizetype>(sourceRow) + count > m_currentPlayingIndex)
            m_currentPlayingIndex = (m_currentPlayingIndex - sourceRow)/*相对拖拽区域行号*/ + destinationChild - count; // Removed the currently playing item
    }
    else { // moving up
        auto to = m_items.begin() + destinationChild;
        auto beg = m_items.begin() + sourceRow;
        auto end = beg + count;
        std::rotate(to, beg, end);
        if (sourceRow <= m_currentPlayingIndex && static_cast<qsizetype>(sourceRow) + count > m_currentPlayingIndex)
            m_currentPlayingIndex = (m_currentPlayingIndex - sourceRow)/*相对拖拽区域行号*/ + destinationChild; // Removed the currently playing item
    }
    endMoveRows();
    return true;
}




bool PlayListItemListModel::insertItem(int row, const PlayListItem& item)
{
    if (!insertRows(row, 1, QModelIndex())) return false;
    return setData(index(row), item, Qt::EditRole);
}

bool PlayListItemListModel::insertFile(int row, const QString& url)
{
    QFileInfo fileInfo(url);
    PlayListItem item{ fileInfo.absoluteFilePath() };
    return insertItem(row, item);
}

QMimeData* PlayListItemListModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.size() <= 0)
        return nullptr;
    QStringList types = mimeTypes();
    if (types.isEmpty())
        return nullptr;
    QMimeData* data = new QMimeData();
    QString format = types.at(APP_PLAYLIST_MIMETYPE_INDEX);
    QByteArray encoded;
    QDataStream stream(&encoded, QDataStream::WriteOnly);
    encodeData(indexes, stream); // 与QAbstractListModel相同
    data->setData(format, encoded);
    return data;
}

bool PlayListItemListModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!(action & supportedDropActions()))
        return false;

    // 检查是否有匹配的MIME类型
    const QStringList modelTypes = mimeTypes();
    for (int i = 0; i < modelTypes.size(); ++i)
        if (data->hasFormat(modelTypes.at(i)))
            return true;
    return false;
}

bool PlayListItemListModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if (!data || !supportedDropActions().testFlag(action))
        return false;

    QStringList types = mimeTypes();
    if (types.isEmpty())
        return false;

#ifdef QT_DEBUG
    auto fmts = data->formats();
    QString formatsLogText = tr("Got drop mime data formats:");
    qDebug().noquote() << formatsLogText;
    for (const QString& fmt : fmts)
        qDebug() << "  -" << fmt;
#endif

    QString format;
    for (auto& mimeType : types) // 按先后顺序查询合适的MIME类型
    {
        if (data->hasFormat(mimeType))
        {
            format = mimeType;
            qDebug() << "Supported drop mime data format:" << mimeType;
        }
    }
    if (format.isEmpty()) // 没有合适的MIME类型
        return false;

    switch (m_mimeTypeIndexMap.value(format, -1))
    {
    case APP_PLAYLIST_MIMETYPE_INDEX:
        return internalDropMimeData(data, format, action, row, column, parent);
    case FILE_URI_LIST_MIMETYPE_INDEX:
        return externalFileDropMimeData(data, format, action, row, column, parent);
    default:
        break;
    }
    return false; // 默认不处理
}

QStringList PlayListItemListModel::mimeTypes() const
{
    return m_mimeTypes;
}

void PlayListItemListModel::sort(SortWay way, Qt::SortOrder order)
{
    beginResetModel();
    if (way == SortWay::ByRandom)
        sortByRandom();
    else
        std::sort(m_items.begin(), m_items.end(), [&](const PlayListItem& a, const PlayListItem& b) {
            switch (way)
            {
            case PlayListItemListModel::ByTitle:
                return sortComparator(a.title, b.title, order);
            case PlayListItemListModel::ByDuration:
                return sortComparator(a.duration, b.duration, order);
            case PlayListItemListModel::ByFileName:
                return sortComparator(a.fileInfo.fileName(), b.fileInfo.fileName(), order);
            case PlayListItemListModel::ByUrl:
                return sortComparator(a.url, b.url, order);
            case PlayListItemListModel::ByFileSize:
                return sortComparator(a.fileInfo.size(), b.fileInfo.size(), order);
            case PlayListItemListModel::ByRandom:
                return sortComparatorRandom();
            default:
                return true;
            }
        });
    // 刷新列表视图
    endResetModel();
}

// copy from QAbstractItemModel::encodeData
void PlayListItemListModel::encodeData(const QModelIndexList& indexes, QDataStream& stream) const
{
    for (const QModelIndex& index : indexes)
        stream << index.row() << index.column() << itemData(index); // 序列化行号、列号和数据
}

// copy from QAbstractItemModel::decodeData
bool PlayListItemListModel::decodeData(int row, int column, const QModelIndex& parent, QDataStream& stream)
{
    int top = INT_MAX;
    int left = INT_MAX;
    int bottom = 0;
    int right = 0;
    QList<int> rows, columns;
    QList<QMap<int, QVariant>> data;

    // 反序列化数据，保存其对应的行列号，并计算整个选区上下左右四个边界
    while (!stream.atEnd()) {
        int r, c;
        QMap<int, QVariant> v;
        stream >> r >> c >> v;
        rows.append(r);
        columns.append(c);
        data.append(v);
        top = qMin(r, top);
        left = qMin(c, left);
        bottom = qMax(r, bottom);
        right = qMax(c, right);
    }

    // insert the dragged items into the table, use a bit array to avoid overwriting items,
    // since items from different tables can have the same row and column
    int dragRowCount = 0;
    int dragColumnCount = right - left + 1;

    // Compute the number of continuous rows upon insertion and modify the rows to match
    QList<int> rowsToInsert(bottom + 1); // 要插入的行数列表，初始化为0
    for (int i = 0; i < rows.size(); ++i) // 类似于原地哈希
        rowsToInsert[rows.at(i)] = 1; // rowsToInsert[来源行号] = 1，表示该行被拖动了，类似于bool变量数组判断是否满足某个条件
    for (int i = 0; i < rowsToInsert.size(); ++i) { // 遍历rowsToInsert
        if (rowsToInsert.at(i) == 1) { // 为1表示该行被拖动了
            rowsToInsert[i] = dragRowCount; // rowsToInsert[来源行号] = 要插入的行号偏移量(即有序列表当前项排在几号)
            ++dragRowCount; // 计算拖动的总行数
        }
    }
    for (int i = 0; i < rows.size(); ++i) // 修改rows列表，使其表示每个拖拽的列表项插入后的行号
        rows[i] = top + rowsToInsert.at(rows.at(i));

    QBitArray isWrittenTo(dragRowCount * dragColumnCount);

    // make space in the table for the dropped data
    //int colCount = columnCount(parent);
    //if (colCount == 0) {
    //    insertColumns(colCount, dragColumnCount - colCount, parent);
    //    colCount = columnCount(parent);
    //}
    insertRows(row, dragRowCount, parent); // 在指定位置插入足够的行数

    // 确保row和column不为负数
    row = qMax(0, row);
    column = qMax(0, column);

    QList<QPersistentModelIndex> newIndexes(data.size()); // 保存新插入项的索引
    // set the data in the table
    for (int j = 0; j < data.size(); ++j) { // 遍历每个拖拽的列表项
        int relativeRow = rows.at(j) - top; // 相对于拖拽区域顶部的行号
        int relativeColumn = columns.at(j) - left; // 相对于拖拽区域左侧的列号
        int destinationRow = relativeRow + row; // 计算插入后的行号
        //int destinationColumn = relativeColumn + column; // 计算插入后的列号
        int flat = (relativeRow * dragColumnCount) + relativeColumn; // 计算一维索引
        // if the item was already written to, or we just can't fit it in the table, create a new row
        if (/*destinationColumn >= colCount || */isWrittenTo.testBit(flat)) {
            //destinationColumn = qBound(column, destinationColumn, colCount - 1);
            destinationRow = row + dragRowCount;
            insertRows(row + dragRowCount, 1, parent);
            flat = (dragRowCount * dragColumnCount) + relativeColumn;
            isWrittenTo.resize(++dragRowCount * dragColumnCount);
        }
        if (!isWrittenTo.testBit(flat)) {
            newIndexes[j] = index(destinationRow, 0/*destinationColumn*/, parent);
            isWrittenTo.setBit(flat);
        }
    }

    for (int k = 0; k < newIndexes.size(); k++) {
        if (newIndexes.at(k).isValid())
        {
            setItemData(newIndexes.at(k), data.at(k));
            // 检测是否需要更新正在播放的列表项索引
            QVariant var = data.at(k).value(Qt::DisplayRole);
            if (var.canConvert<PlayListItem>())
            {
                PlayListItem item = var.value<PlayListItem>();
                qsizetype newIndex = newIndexes.at(k).row();
                if (item.playing) m_currentPlayingIndex = newIndex;
            }
        }
    }

    return true;
}

bool PlayListItemListModel::internalDropMimeData(const QMimeData* data, const QString& format, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    QByteArray encoded = data->data(format);
    QDataStream stream(&encoded, QDataStream::ReadOnly);

    QModelIndex newParent = parent;
    // if the drop is on an item
    if (newParent.isValid() && row == -1 && column == -1)
    {
        // 返回false取消操作
        //return false;
        int top = INT_MAX;
        int left = INT_MAX;
        QList<int> rows, columns;
        QList<QMap<int, QVariant>> data;
        while (!stream.atEnd())
        {
            int r, c;
            QMap<int, QVariant> v;
            stream >> r >> c >> v;
            rows.append(r);
            columns.append(c);
            data.append(v);
            top = qMin(r, top);
            left = qMin(c, left);
        }
        stream.device()->seek(0); // 回到数据流开始位置
        stream.resetStatus(); // 重置流状态
        if (top > newParent.row()) // 来源index > 目标列表项index，即向上移动
            row = newParent.row();
        else if (action & Qt::MoveAction) // 向下移动，移动时需计算插入位置时要加列表项数目，并且row要确保 < rowCount()
        {
            row = newParent.row() + rows.size(); // 计算插入位置
            int rows = rowCount(QModelIndex());
            if (row >= rows)
                row = rows;// 确保row不超过最大范围
        }

        newParent = QModelIndex();
    }

    // if the drop is on an item, replace the data in the items
    //if (parent.isValid() && row == -1 && column == -1) {
    //    int top = INT_MAX;
    //    int left = INT_MAX;
    //    QList<int> rows, columns;
    //    QList<QMap<int, QVariant>> data;
    //    while (!stream.atEnd()) {
    //        int r, c;
    //        QMap<int, QVariant> v;
    //        stream >> r >> c >> v;
    //        rows.append(r);
    //        columns.append(c);
    //        data.append(v);
    //        top = qMin(r, top);
    //        left = qMin(c, left);
    //    }
    //    // 每一项按顺序并保持相对位置不变覆盖已有项的数据
    //    for (int i = 0; i < data.size(); ++i) {
    //        int r = (rows.at(i) - top) + parent.row();
    //        if (columns.at(i) == left && hasIndex(r, 0))
    //            setItemData(index(r), data.at(i));
    //    }
    //    return true;
    //}

    if (row == -1)
        row = rowCount(newParent);

    // otherwise insert new rows for the data
    return decodeData(row, column, newParent, stream); // 与QAbstractListModel差不多，但是删除了column部分
}

bool PlayListItemListModel::externalFileDropMimeData(const QMimeData* data, const QString& format, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    // 添加到列表
    QList<QUrl> urls = data->urls();
    
    QModelIndex newParent = parent;
    // if the drop is on an item
    if (newParent.isValid() && row == -1 && column == -1)
        row = newParent.row(); // 插入到当前列表项位置
    if (row == -1)
        row = rowCount(parent);
    for (auto& url : urls)
        insertFile(row, url.toLocalFile());
    return false; // 返回false，表示不进行任何操作，如移动时外部(explorer等)不自动删除源文件
}

bool PlayListItemListModel::sortComparator(const QString& a, const QString& b, Qt::SortOrder order)
{
    switch (order)
    {
    case Qt::AscendingOrder: // 升序
        if (strcmp(a.toStdString().c_str(), b.toStdString().c_str()) < 0)
            return true;
        return false;
    case Qt::DescendingOrder: // 降序
        if (strcmp(a.toStdString().c_str(), b.toStdString().c_str()) > 0)
            return true;
        return false;
    default:
        return true;
    }
}

bool PlayListItemListModel::sortComparator(const size_t& a, const size_t& b, Qt::SortOrder order)
{
    switch (order)
    {
    case Qt::AscendingOrder: // 升序
        if (a < b) return true;
        return false;
    case Qt::DescendingOrder: // 降序
        if (a > b) return true;
        return false;
    default:
        return true;
    }
}

bool PlayListItemListModel::sortComparatorRandom()
{
    static std::random_device rd;
    static std::mt19937_64 engine(rd());
    std::uniform_int_distribution<int> dist(0, 1); // [0, 1]
    return dist(engine);
}

void PlayListItemListModel::sortByRandom()
{
    static std::random_device rd;
    static std::mt19937_64 engine(rd());
    std::uniform_int_distribution<qsizetype> dist(0, m_items.size() - 1); // [0, m_items.size() - 1]
    QList<PlayListItem> newList(m_items.size());
    std::unordered_set<qsizetype> usedIndexes;
    for (qsizetype i = 0; i < m_items.size(); ++i)
    {
        auto index = dist(engine);
        while (usedIndexes.count(index))
            index = dist(engine);
        newList[i] = m_items[index];
        usedIndexes.emplace(index);
    }
    m_items = newList;
}

void PlayListListViewItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QVariant selectionVar = index.data(Qt::BackgroundRole);
    QFontMetrics fontMetrics(opt.font);
    QVariant var = index.data(Qt::DisplayRole);
    PlayListItem item = var.value<PlayListItem>();
    QRect itemRect = opt.rect.adjusted(paddingInsideSize, paddingInsideSize, -paddingInsideSize, -paddingInsideSize);
    // 处理滚动条遮挡问题
    QSize verticalScrollBarSize = getScrollBarSize(opt.widget, Qt::Vertical);
    if (verticalScrollBarSize.isValid())
        itemRect.setWidth(itemRect.width() - verticalScrollBarSize.width());
    // 选中或者有焦点时，绘制背景
    //if ((opt.state & QStyle::State_Selected) || (opt.state & QStyle::State_MouseOver) || (opt.state & QStyle::State_HasFocus))
    //{
    //    painter->save();
    //    painter->setPen(Qt::NoPen);
    //    painter->setBrush(opt.palette.brush(QPalette::ColorRole::AlternateBase));
    //    painter->drawRoundedRect(itemRect, itemRadius, itemRadius);
    //    painter->setPen(opt.palette.color(QPalette::Text));
    //    painter->setBrush(Qt::NoBrush);
    //    painter->restore();
    //}
    //else
        QStyledItemDelegate::paint(painter, opt, index);

    painter->save();
    // draw icon
    QSize iconSize{ option.decorationSize };
    QRect iconRect{ itemRect.left() + paddingSize, itemRect.top() + (itemRect.height() - iconSize.height()) / 2, iconSize.width(), iconSize.height() };
    item.icon.paint(painter, iconRect);
    // draw text
    // 设置画笔颜色
    QVariant&& foregroundVar = index.data(Qt::ForegroundRole);
    if (foregroundVar.canConvert<QColor>())
    {
        QColor foregroundColor = foregroundVar.value<QColor>();
        painter->setPen(foregroundColor);
    }
    QRect titleDrawRect = itemRect.adjusted(iconSize.width() + fontMetrics.averageCharWidth() + paddingSize, paddingSize, -paddingSize, -paddingSize);
    QString titleDrawText = fontMetrics.elidedText(item.fileInfo.fileName(), Qt::ElideRight, titleDrawRect.width());
    QTextOption titleTextOption(Qt::AlignVCenter | Qt::AlignLeft);
    titleTextOption.setWrapMode(QTextOption::NoWrap);
    painter->drawText(titleDrawRect, titleDrawText, titleTextOption);
    painter->restore();
    //QVariant checkState = index.data(Qt::CheckStateRole);
}

QSize PlayListListViewItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    return QSize(option.rect.width(), option.fontMetrics.height() + paddingSize * 3);
}

QSize PlayListListViewItemDelegate::getScrollBarSize(const QWidget* widget, Qt::Orientation orientation) const
{
    if (!widget) return QSize();
    const QAbstractScrollArea* scrollArea = qobject_cast<const QAbstractScrollArea*>(widget);
    if (!scrollArea) return QSize();
    QScrollBar* scrollBar = nullptr;
    if (orientation == Qt::Horizontal)
        scrollBar = scrollArea->horizontalScrollBar();
    else if (orientation == Qt::Vertical)
        scrollBar = scrollArea->verticalScrollBar();
    if (!scrollBar) return QSize();
    if (scrollBar->isVisible())
        return scrollBar->size();
    return QSize();
}



//QMap<int, QVariant> QStringListModel::itemData(const QModelIndex& index) const
//{
//    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
//        return QMap<int, QVariant>{};
//    const QVariant displayData = lst.at(index.row());
//    return QMap<int, QVariant>{{
//            std::make_pair<int>(Qt::DisplayRole, displayData),
//            std::make_pair<int>(Qt::EditRole, displayData)
//        }};
//}

/*!
  \reimp
  \since 5.13
  If \a roles contains both Qt::DisplayRole and Qt::EditRole, the latter will take precedence
*/
//bool QStringListModel::setItemData(const QModelIndex& index, const QMap<int, QVariant>& roles)
//{
//    if (roles.isEmpty())
//        return false;
//    if (std::any_of(roles.keyBegin(), roles.keyEnd(), [](int role) -> bool {
//        return role != Qt::DisplayRole && role != Qt::EditRole;
//        })) {
//        return false;
//    }
//    auto roleIter = roles.constFind(Qt::EditRole);
//    if (roleIter == roles.constEnd())
//        roleIter = roles.constFind(Qt::DisplayRole);
//    Q_ASSERT(roleIter != roles.constEnd());
//    return setData(index, roleIter.value(), roleIter.key());
//}

PlayListListView::PlayListListView(QWidget* parent)
    : QListView(parent)
{
    PlayListListViewItemDelegate* delegate = new PlayListListViewItemDelegate(this);
    this->setItemDelegate(delegate);
    this->setAutoFillBackground(true);
}

void PlayListListView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QModelIndex index = this->indexAt(event->pos());
    if (!index.isValid())
        emit backgroundDoubleClicked();
    QListView::mouseDoubleClickEvent(event);
}
