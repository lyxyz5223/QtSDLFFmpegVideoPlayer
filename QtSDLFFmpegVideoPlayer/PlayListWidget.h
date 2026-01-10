#pragma once
#include <qwidget.h>
#include "ui_PlayListWidget.h"
#include <qicon.h>
#include <qstringlistmodel.h>
#include <Logger.h>
#include "PlayerPredefine.h"

struct PlayListTypes
{
    struct PlayListItem
    {
        QString url;
        QString title;
        QIcon icon;
    };
    using PlayList = QList<PlayListItem>;
};

class PlayListWidget : public QWidget, public PlayListTypes
{
    Q_OBJECT

public:
    PlayListWidget(QWidget* parent = nullptr);
    ~PlayListWidget();
private:
    DefinePlayerLoggerSinks(PlayListWidgetLoggerSinks, "PlayListWidget");
    Logger logger{ "PlayListWidget" };

    PlayList m_playList;
    QStringListModel* m_playListModel{ nullptr };

    std::function<void(const QModelIndex&)> playCallback;

public:
    void setPlayList(PlayList& playlist);
    void setPlayCallback(std::function<void(const QModelIndex&)> callback) {
        playCallback = callback;
    }
    const PlayList& playList() const { return m_playList; }
    PlayListItem getPlayListItem(const QModelIndex& index) const {
        return m_playList.at(index.row());
    }

protected:

signals:

public slots:
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
    void appendPlayListItem(const PlayListItem& item);
};

