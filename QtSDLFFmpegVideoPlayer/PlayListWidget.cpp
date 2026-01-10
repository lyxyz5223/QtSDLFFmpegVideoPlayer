#include "PlayListWidget.h"
#include <QFileDialog>
#include <QFileIconProvider>


PlayListWidget::PlayListWidget(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    //ui.listItems->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    m_playListModel = new QStringListModel(this);
    ui.listItems->setModel(m_playListModel);
}

PlayListWidget::~PlayListWidget()
{

}

void PlayListWidget::setPlayList(PlayList& playlist)
{
    //m_playList = playlist;
    m_playList.clear();
    m_playListModel->setStringList(QStringList()); // 清空模型
    for (auto& item : playlist)
        appendPlayListItem(item);
}

void PlayListWidget::itemAdd()
{
    QStringList&& files = selectFiles();
    if (files.isEmpty()) return;
    for (auto& file : files)
    {
        QFileInfo fileInfo(file);
        QFileIconProvider iconProvider;
        PlayListItem item{ fileInfo.absoluteFilePath(), fileInfo.fileName(), iconProvider.icon(fileInfo) };
        appendPlayListItem(item);
    }
}

void PlayListWidget::itemRemove()
{
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

void PlayListWidget::appendPlayListItem(const PlayListItem& item)
{
    m_playList.append(item);
    m_playListModel->insertRow(ui.listItems->model()->rowCount());
    m_playListModel->setData(m_playListModel->index(m_playListModel->rowCount() - 1), item.title, Qt::DisplayRole);
}

