#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtSDLFFmpegVideoPlayer.h"
#include <thread>
#include "SDLWidget.h"
#include <SDLMediaPlayer.h>
#include <QList>

class QtSDLFFmpegVideoPlayer : public QMainWindow
{
    Q_OBJECT

    class QtSDLMediaPlayer : public SDLMediaPlayer {
        QtSDLFFmpegVideoPlayer& owner;
        AtomicBool inited = false;
    public:
        QtSDLMediaPlayer(QtSDLFFmpegVideoPlayer& owner) : SDLMediaPlayer(), owner(owner) {}
        // 重写事件处理函数
        virtual void stopEvent(MediaPlaybackStateChangeEvent* e) override {
            SDLMediaPlayer::stopEvent(e);
            inited.store(false); // 重置初始化状态
            owner.afterPlaybackCallback();
        }
        virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
            SDLMediaPlayer::startEvent(e);
            inited.store(false); // 重置初始化状态
        }
        virtual void videoRenderEvent(VideoRenderEvent* e) override {
            SDLMediaPlayer::videoRenderEvent(e);
            if (!inited.load())
            {
                owner.beforePlaybackCallback(e->frameContext()->formatCtx, e->frameContext()->codecCtx);
                inited.store(true);
            }
            // 转发渲染事件
            owner.renderCallback(*e->frameContext());
        }
    };

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
    QtSDLMediaPlayer mediaPlayer{ *this };

    int64_t videoDuration = 0;

    // 
    std::atomic<bool> isMediaSliderMoving = false;
    int mediaSliderMovingTo = 0;

    // 渲染第一帧的时候调用
    void beforePlaybackCallback(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx);
    // 每一帧渲染时调用
    void renderCallback(const MediaPlayer::VideoFrameContext& frameCtx);
    // 播放停止后（无论什么原因）调用
    void afterPlaybackCallback();

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
