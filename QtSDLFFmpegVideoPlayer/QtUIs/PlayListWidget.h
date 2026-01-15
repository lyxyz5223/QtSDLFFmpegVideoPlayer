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

    std::function<void(const QModelIndex&)> playCallback;

public:
    void setPlayList(QList<PlayListItem>& playlist);
    void setPlayList(QList<QString>& urls);
    void setPlayCallback(std::function<void(const QModelIndex&)> callback) {
        playCallback = callback;
    }
    const QList<PlayListItem>& playList() const { return m_playListModel->playList(); }
    qsizetype getPlayListSize() const { return m_playListModel->playList().size(); }
    PlayListItem getPlayListItem(const QModelIndex& index) const {
        return m_playListModel->playList().at(index.row());
    }
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

    void appendFiles() {
        itemAdd(); // 打开文件选择对话框，并添加到播放列表
    }
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

private slots:
    void itemAdd();
    void itemRemove();
    void itemsClear();

    void itemSearch();

    void itemActivated(const QModelIndex& index);
    void itemClicked(const QModelIndex& index);
    void itemDoubleClicked(const QModelIndex& index);
private:
    Ui_PlayListWidgetClass ui;

    QStringList selectFiles();
};

