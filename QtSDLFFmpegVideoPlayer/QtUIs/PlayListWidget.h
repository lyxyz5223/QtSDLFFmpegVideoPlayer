#pragma once
#include <qwidget.h>
#include "ui_PlayListWidget.h"
#include <qicon.h>
#include <qstringlistmodel.h>
#include <Logger.h>
#include "PlayerPredefine.h"
#include "PlayListListView.h"

class PlayListWidget : public QWidget
{
    Q_OBJECT

public:
    PlayListWidget(QWidget* parent = nullptr);
    ~PlayListWidget();
private:
    DefinePlayerLoggerSinks(PlayListWidgetLoggerSinks, "PlayListWidget");
    Logger logger{ "PlayListWidget" };

    PlayListItemListModel* m_playListModel{ nullptr };


public:
    void setPlayList(QList<PlayListItem>& playlist);
    void setPlayList(QList<QString>& urls);

    const QList<PlayListItem>& playList() const { return m_playListModel->playList(); }
    qsizetype getPlayListSize() const { return m_playListModel->playList().size(); }
    PlayListItem getPlayListItem(qsizetype index) const {
        return m_playListModel->playList().at(index);
    }

    void setCurrentPlayingIndex(qsizetype index) {
        m_playListModel->setCurrentPlayingIndex(index); // 会触发视图更新
    }

    qsizetype getCurrentPlayingIndex() const {
        return m_playListModel->currentPlayingIndex();
    }

    qsizetype getNextPlayingIndex(bool* overflow = nullptr) const;

    qsizetype getPreviousPlayingIndex(bool* overflow = nullptr) const;

    void appendFiles(const QStringList& urls) {
        for (auto& url : urls)
            appendFile(url);
    }
    void appendFile(const QString& url) {
        m_playListModel->appendFile(url);
    }
    void appendItem(const PlayListItem& item) {
        m_playListModel->appendItem(item);
    }

protected:
    virtual void resizeEvent(QResizeEvent* e) override;

signals:
    void play(qsizetype index); // 播放当前选中项

public slots:
    void appendFiles();
    void appendFolder();
    void removeSelectedItems();
    void clearListItems();

    void searchItems();
private slots:
    void itemActivated(const QModelIndex& index);
    void itemClicked(const QModelIndex& index);
    void itemDoubleClicked(const QModelIndex& index);

private:
    Ui_PlayListWidgetClass ui;

    QStringList selectFiles();
    QString selectFolder();
    QStringList listFilesInFolder(const QString& folderPath);
    void updateUISearchTotalNumberLabel();
    void updateUISearchFoundNumberLabel(qsizetype foundNumber);

};

