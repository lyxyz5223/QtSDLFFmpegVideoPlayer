#include "PlayListWidget.h"
#include <QFileDialog>

PlayListWidget::PlayListWidget(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_playListModel = new PlayListItemListModel(this);
    ui.listItems->setModel(m_playListModel);
    PlayListListViewItemDelegate* delegate = new PlayListListViewItemDelegate(this);
    ui.listItems->setItemDelegate(delegate);

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

void PlayListWidget::itemAdd()
{
    QStringList&& files = selectFiles();
    if (files.isEmpty()) return;
    appendFiles(files);
}

void PlayListWidget::itemRemove()
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

void PlayListWidget::itemsClear()
{
}

void PlayListWidget::itemSearch()
{
}

void PlayListWidget::itemActivated(const QModelIndex& index)
{

}

void PlayListWidget::itemClicked(const QModelIndex& index)
{

}

void PlayListWidget::itemDoubleClicked(const QModelIndex& index)
{
    if (playCallback)
        playCallback(index);
}

QStringList PlayListWidget::selectFiles()
{
    // 打开文件对话框
    QFileDialog fileDialog(this, tr("Open Files"), "", tr("Video Files (*.mp4 *.avi *.mkv *.mov *.flv *.wmv);;All Files (*)"));
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    if (fileDialog.exec() == QFileDialog::DialogCode::Rejected)
        return {};
    QStringList files = fileDialog.selectedFiles();
    return files;
}


