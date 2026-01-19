#include "PlayListWidget.h"
#include <QFileDialog>
#include <QResizeEvent>
#include <QDir>
#include <QRegularExpression>
#include "AnimatedMenu.h"

PlayListWidget::PlayListWidget(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_playListModel = new PlayListItemListModel(this);
    ui.listItems->setModel(m_playListModel);

    connect(m_playListModel, &QAbstractListModel::rowsInserted, [this](const QModelIndex& parent, int first, int last) {
        updateUISearchTotalNumberLabel();
        searchItems();
    });
    // test
    //QFileInfo fileInfo("D:\\Softwares\\Jijidown\\Download\\「鏡音鈴」「Gimme×Gimme」 Sour式鏡音Rin×Sour式初音Miku[PV] - 1.gimme bili(Av84791490,P1).mp4");
    //appendFile(fileInfo.absoluteFilePath());
}

PlayListWidget::~PlayListWidget()
{

}

void PlayListWidget::setPlayList(QList<PlayListItem>& playlist)
{
    //m_playList = playlist;
    m_playListModel->clear(); // 清空模型
    for (auto& item : playlist)
        appendItem(item);
}
void PlayListWidget::setPlayList(QList<QString>& urls)
{
    //m_playList = playlist;
    m_playListModel->clear(); // 清空模型
    for (auto& url : urls)
        appendFile(url);
}

qsizetype PlayListWidget::getNextPlayingIndex(bool* overflow) const
{
    auto curIdx = getCurrentPlayingIndex();
    auto playListSize = getPlayListSize();
    if (playListSize > 0 && curIdx == playListSize - 1) // 隐含curIdx != -1
    {
        if (overflow) *overflow = true;
        curIdx = 0; // 当前是列表末尾，切换到列表开头
    }
    else
    {
        if (overflow) *overflow = false;
        ++curIdx;
    }
    return curIdx;
}

qsizetype PlayListWidget::getPreviousPlayingIndex(bool* overflow) const
{
    auto curIdx = getCurrentPlayingIndex();
    auto playListSize = getPlayListSize();
    if (playListSize > 0 && curIdx == 0) // 隐含curIdx != -1
    {
        if (overflow) *overflow = true;
        curIdx = playListSize - 1; // 当前是列表开头，切换到列表末尾
    }
    else
    {
        if (overflow) *overflow = false;
        --curIdx;
    }
    return curIdx;
}

void PlayListWidget::resizeEvent(QResizeEvent* e)
{
    QSize oldSize = e->oldSize();
    QSize newSize = e->size();
    //if (newSize.width() / newSize.height() > 3)
    //    ui.listItems->setViewMode(QListView::IconMode);
    //else
    //    ui.listItems->setViewMode(QListView::ListMode);
    QWidget::resizeEvent(e);
}

void PlayListWidget::appendFiles()
{
    QStringList&& files = selectFiles();
    if (files.isEmpty()) return;
    appendFiles(files);
}

void PlayListWidget::appendFolder()
{
    QString folderPath = selectFolder();
    if (folderPath.isEmpty()) return;
    QStringList&& files = listFilesInFolder(folderPath);
    if (files.isEmpty()) return;
    appendFiles(files);
}

void PlayListWidget::removeSelectedItems()
{
    auto indexes = ui.listItems->selectionModel()->selectedIndexes();
    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() > b.row();
        });
    for (auto& index : indexes)
    {
        if (index.isValid() && index.row() < m_playListModel->rowCount())
        {
            m_playListModel->removeRow(index.row());
        }
    }
}

void PlayListWidget::clearListItems()
{
    m_playListModel->clear();
}

void PlayListWidget::searchItems()
{
    QString searchText = ui.editSearchBox->text();
    QRegularExpression regex{ searchText, QRegularExpression::CaseInsensitiveOption };
    qsizetype foundNumber{ 0 };
    for (qsizetype i = 0; i < m_playListModel->rowCount(); ++i)
    {
        QModelIndex index = m_playListModel->index(i, 0);
        QString itemText = m_playListModel->data(index, Qt::DisplayRole).value<PlayListItem>().title;
        bool match = regex.match(itemText).hasMatch();
        ui.listItems->setRowHidden(i, !match);
        if (match) ++foundNumber;
    }
    updateUISearchFoundNumberLabel(foundNumber);
}

void PlayListWidget::itemActivated(const QModelIndex& index)
{

}

void PlayListWidget::itemClicked(const QModelIndex& index)
{

}

void PlayListWidget::itemDoubleClicked(const QModelIndex& index)
{
    emit play(index.row());
}

void PlayListWidget::moveItemUpToTop()
{
    moveItems(-1, true);
}

void PlayListWidget::moveItemUp()
{
    moveItems(-1, false);
}

void PlayListWidget::moveItemDown()
{
    moveItems(1, false);
}

void PlayListWidget::moveItemDownToBottom()
{
    moveItems(1, true);
}

void PlayListWidget::sortItems()
{
    // 打开排序菜单
    AnimatedMenu sortMenu{ this };
    sortMenu.addAction(tr("按标题（升序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByTitle, Qt::AscendingOrder); });
    sortMenu.addAction(tr("按标题（降序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByTitle, Qt::DescendingOrder); });
    sortMenu.addAction(tr("按文件名（升序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByFileName, Qt::AscendingOrder); });
    sortMenu.addAction(tr("按文件名（降序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByFileName, Qt::DescendingOrder); });
    sortMenu.addAction(tr("按时长（升序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByDuration, Qt::AscendingOrder); });
    sortMenu.addAction(tr("按时长（降序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByDuration, Qt::DescendingOrder); });
    sortMenu.addAction(tr("按文件大小（升序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByFileSize, Qt::AscendingOrder); });
    sortMenu.addAction(tr("按文件大小（降序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByFileSize, Qt::DescendingOrder); });
    sortMenu.addAction(tr("按文件路径（升序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByUrl, Qt::AscendingOrder); });
    sortMenu.addAction(tr("按文件路径（降序）"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByUrl, Qt::DescendingOrder); });
    sortMenu.addAction(tr("随机"), this, [this]() { m_playListModel->sort(PlayListItemListModel::ByRandom); });
    sortMenu.exec(ui.btnSortItems->mapToGlobal(QPoint(ui.btnSortItems->size().width(), ui.btnSortItems->size().height())));
}

QStringList PlayListWidget::selectFiles()
{
    // 打开文件对话框
    QFileDialog fileDialog(this, tr("Open Files"), tr(""), tr("Video Files (*.mp4 *.avi *.mkv *.mov *.flv *.wmv);;All Files (*)"));
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    if (fileDialog.exec() == QFileDialog::DialogCode::Rejected)
        return {};
    QStringList files = fileDialog.selectedFiles();
    return files;
}

QString PlayListWidget::selectFolder()
{
    // 打开文件夹对话框
    QFileDialog folderDialog(this, tr("Open Folder"), tr(""));
    folderDialog.setFileMode(QFileDialog::Directory);
    if (folderDialog.exec() == QFileDialog::DialogCode::Rejected) return {};
    QStringList files = folderDialog.selectedFiles();
    if (files.isEmpty()) return {};
    return files[0];
}

QStringList PlayListWidget::listFilesInFolder(const QString& folderPath)
{
    QDir dir{ folderPath };
    if (!dir.exists()) return {};
    QStringList nameFilters;
    //nameFilters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.flv" << "*.wmv";
    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters, QDir::Files | QDir::NoSymLinks);
    QStringList filePaths;
    for (const QFileInfo& fileInfo : fileInfoList)
        filePaths.append(fileInfo.absoluteFilePath());
    return filePaths;
}

void PlayListWidget::updateUISearchTotalNumberLabel()
{
    ui.labelSearchTotalNumber->setText(QString::number(m_playListModel->rowCount()));
}

void PlayListWidget::updateUISearchFoundNumberLabel(qsizetype foundNumber)
{
    ui.labelSearchFoundNumber->setText(QString::number(foundNumber));
}

void PlayListWidget::moveItems(int64_t offset, bool most)
{
    if (offset == 0)
        return;
    auto selModel = ui.listItems->selectionModel();
    auto selIdxes = selModel->selectedIndexes();
    if (selIdxes.isEmpty())
        return;
    std::sort(selIdxes.begin(), selIdxes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() < b.row();
        });
    if (!most)
    {
        auto rowCount = m_playListModel->rowCount();
        auto moveRow = [this, &selModel, &offset, &rowCount](const QModelIndex& idx) -> bool {
            if (!idx.isValid() || idx.row() >= rowCount)
                return false;
            int destRow = idx.row() + offset;
            if (destRow < 0 || destRow >= rowCount)
                return false;
            m_playListModel->moveRows(QModelIndex(), idx.row(), 1, QModelIndex(), destRow + ((offset > 0) ? 1 : 0));
            selModel->select(m_playListModel->index(destRow, 0), QItemSelectionModel::Select);
            return true;
        };
        if (offset < 0)
        {
            if (selIdxes.front().row() != 0)
                for (auto& idx : selIdxes)
                    if (!moveRow(idx)) continue;
        }
        else
        {
            if (selIdxes.back().row() != rowCount - 1)
                for (qsizetype i = selIdxes.size(); i > 0; --i)
                {
                    auto& idx = selIdxes[i - 1];
                    if (!moveRow(idx)) continue;
                }
        }
        return;
    }
    if (offset < 0)
    {
        size_t idxOffset{ 0 };
        for (auto& idx : selIdxes)
        {
            if (!idx.isValid() || idx.row() >= m_playListModel->rowCount())
                continue;
            m_playListModel->moveRows(QModelIndex(), idx.row(), 1, QModelIndex(), idxOffset);
            selModel->select(m_playListModel->index(idxOffset, 0), QItemSelectionModel::Select);
            ++idxOffset;
        }
    }
    else
    {
        size_t idxOffset{ 0 };
        for (auto& idx : selIdxes)
        {
            auto rowCount = m_playListModel->rowCount();
            if (!idx.isValid() || idx.row() >= rowCount)
                continue;
            m_playListModel->moveRows(QModelIndex(), idx.row() - idxOffset, 1, QModelIndex(), rowCount);
            selModel->select(m_playListModel->index(rowCount - 1, 0), QItemSelectionModel::Select);
            ++idxOffset;
        }
    }
}


