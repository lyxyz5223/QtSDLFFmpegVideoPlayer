#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtSDLFFmpegVideoPlayer.h"
#include <thread>
#include "SDLWidget.h"
#include "SDLMediaPlayer.h"
#include <QList>

class QtSDLFFmpegVideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    QtSDLFFmpegVideoPlayer(QWidget *parent = nullptr);
    ~QtSDLFFmpegVideoPlayer();

    void setArgcArgv(int argc, char** argv) {
        this->argc = argc;
        this->argv = argv;
    }

protected:
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    virtual void closeEvent(QCloseEvent* e) override;


public slots:
    void sdlEventLoop();

    void selectFile();

    void selectFileAndPlay();

    void mediaPlayPause();
    void mediaStop();
    void mediaPrevious();
    void mediaNext();

    void mediaSeek();


    void mediaSliderMoved(int);
    void mediaSliderPressed();
    void mediaSliderReleased();
    void mediaSliderValueChanged(int);

private:
    Ui::QtSDLFFmpegVideoPlayerClass ui;
    int argc = 0;
    char** argv = nullptr;

    SDLWidget* sdlWidget = nullptr;
    QTimer* sdlEventTimer;
    SDLMediaPlayer mediaPlayer;

    int64_t videoDuration = 0;

    // 
    std::atomic<bool> isMediaSliderMoving = false;
    int mediaSliderMovingTo = 0;

    void beforePlaybackCallback(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoPlayer::UserDataType userData);
    void renderCallback(const MediaPlayer::VideoFrameContext& frameCtx, MediaPlayer::VideoUserDataType userData);
    void afterPlaybackCallback(VideoPlayer::UserDataType userData);

    constexpr static auto SDLPollEventFunction = []() -> int {
        SDL_Event e;
        return SDL_PollEvent(&e);
        };


    struct PlayListItem
    {
        std::string filePath;
    };
    QList<PlayListItem> vecPlayList;

};
