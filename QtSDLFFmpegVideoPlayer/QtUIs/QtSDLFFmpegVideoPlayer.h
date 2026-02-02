#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtSDLFFmpegVideoPlayer.h"
#include <thread>
#include <QList>


// Windows任务栏媒体控制集成
#include "TaskbarMediaController.h"

#ifdef USE_SDL_WIDGET
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
#else
#endif

#ifdef USE_SDL_WIDGET
#include "SDLWidget.h"
#include <SDLMediaPlayer.h>
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
#include <QtMultimediaWidgets>
#include <QtMediaPlayer.h>
#else
#endif

#ifdef USE_SDL_WIDGET
using TargetMediaPlayer = SDLMediaPlayer;
using VideoWidget = SDLWidget;
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
using TargetMediaPlayer = QtMultiMediaPlayer;
using VideoWidget = QVideoWidget;
#else
using TargetMediaPlayer = MediaPlayer;
#endif



class QtSDLFFmpegVideoPlayer : public QMainWindow
{
    Q_OBJECT

    friend class QtSDLMediaPlayer;
    class QtSDLMediaPlayer : public TargetMediaPlayer {
        QtSDLFFmpegVideoPlayer& owner;
        AtomicBool inited = false;
        AtomicBool stopped = true;
        AtomicInt seekingCount{ 0 };

    public:
        QtSDLMediaPlayer(QtSDLFFmpegVideoPlayer& owner) : TargetMediaPlayer(), owner(owner) {}
        // 重写事件处理函数
        virtual void stopEvent(MediaPlaybackStateChangeEvent* e) override {
            TargetMediaPlayer::stopEvent(e);
            stopped.store(true);
            inited.store(false); // 重置初始化状态
            owner.afterPlaybackCallback();
            owner.setPlayPauseButtonState(false);
        }
        virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) override {
            TargetMediaPlayer::pauseEvent(e);
            owner.setPlayPauseButtonState(false);
        }
        virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) override {
            TargetMediaPlayer::resumeEvent(e);
            owner.setPlayPauseButtonState(true);
        }
        virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
            TargetMediaPlayer::startEvent(e);
            inited.store(false); // 重置初始化状态
            stopped.store(false);
            owner.setPlayPauseButtonState(true);
        }
        virtual void seekEvent(MediaSeekEvent* e) override {
            switch (e->handleState())
            {
            case RequestHandleState::BeforeEnqueue:
                break;
            case RequestHandleState::AfterEnqueue:
                seekingCount.fetch_add(1);
                break;
            case RequestHandleState::BeforeHandle:
                owner.setPlayPauseButtonState(false);
                break;
            case RequestHandleState::AfterHandle:
                seekingCount.fetch_sub(1);
                owner.setPlayPauseButtonState(true);
                break;
            default:
                break;
            }
        }
        virtual void videoRenderEvent(VideoRenderEvent* e) override {
            TargetMediaPlayer::videoRenderEvent(e);
            if (stopped.load() || seekingCount.load() != 0)
                return;
            auto ctx = e->frameContext();
            if (!inited.load())
            {
                if (stopped.load())
                    return;
                inited.store(true);
                owner.beforePlaybackCallback(ctx->formatCtx, ctx->codecCtx);
            }
            // 转发渲染事件
            owner.videoRenderCallback(*ctx);
        }
        virtual void audioRenderEvent(AudioRenderEvent* e) override {
            TargetMediaPlayer::audioRenderEvent(e);
            if (stopped.load() || seekingCount.load() != 0)
                return;
            auto ctx = e->frameContext();
            if (!inited.load())
            {
                if (stopped.load())
                    return;
                inited.store(true);
                owner.beforePlaybackCallback(ctx->formatCtx, ctx->codecCtx);
            }
            // 转发渲染事件
            owner.audioRenderCallback(*ctx);
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
    virtual void resizeEvent(QResizeEvent* e) override;


public slots:
    void sdlEventLoop();

    void selectFilesAndPlay();
    void selectFolderAndPlay();
    void selectUrlAndPlay();
    void openSettingsDialog();

    void mediaPlayPause();
    void mediaStop();
    void mediaPrevious();
    void mediaNext();

    void mediaSeek();


    void mediaSliderMoved(int);
    void mediaSliderPressed();
    void mediaSliderReleased();
    void mediaSliderValueChanged(int);

    void mediaVolumeMuteUnmute();
    void mediaVolumeSliderMoved(int);
    void mediaVolumeSliderValueChanged(int);

    void mediaOpenPlayOptionsDialog();

private:
    Ui::QtSDLFFmpegVideoPlayerClass ui;
    int argc = 0;
    char** argv = nullptr;

    // 日志记录
    DefinePlayerLoggerSinks(qtSDLFFmpegVideoPlayerLoggerSinks, "QtSDLFFmpegVideoPlayer");
    Logger logger{ "QtSDLFFmpegVideoPlayer", qtSDLFFmpegVideoPlayerLoggerSinks };

    TaskbarMediaController taskbarMediaController;

    // SDL渲染窗口
#ifdef USE_SDL_WIDGET
    std::unique_ptr<VideoWidget> videoWidget{ nullptr };
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
    VideoWidget* videoWidget{ nullptr };
    QMediaPlayer* qMediaPlayer{ nullptr };
    QAudioOutput* qAudioOutput{ nullptr };
#else
    std::unique_ptr<VideoWidget> videoWidget{ nullptr };
#endif
    QTimer* sdlEventTimer{ nullptr }; // SDL事件轮询定时器，只执行一次，回调中循环
    QtSDLMediaPlayer mediaPlayer{ *this }; // 媒体播放器实例

    int64_t duration{ 0 }; // 媒体总时长，单位毫秒，滑块的int类型大约可以存储24.85513480324074074...天
    int64_t currentTimeS{ 0 }; // 当前播放位置，单位秒

    // 
    std::atomic<bool> isMediaSliderMoving{ false };
    int mediaSeekingTime{ 0 };

    void createVideoWidget() {
#ifdef USE_SDL_WIDGET
        videoWidget = std::make_unique<VideoWidget>((HWND)ui.videoRenderWidget->winId(), "direct3d11");
        videoWidget->show();
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
        videoWidget = new VideoWidget(ui.videoRenderWidget);
        ui.videoRenderWidget->layout()->addWidget(videoWidget);
#else
#endif
    }
    void destroyVideoWidget() {
    }
    void moveVideoWidget(QPoint pos) {
#ifdef USE_SDL_WIDGET
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
        videoWidget->move(pos);
#endif
    }
    void resizeVideoWidget(QSize size) {
#ifdef USE_SDL_WIDGET
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
        videoWidget->resize(size);
#endif
    }

    // 设置按钮为播放或暂停状态
    void setPlayPauseButtonState(bool isPlaying) {
        if (isPlaying)
            ui.btnPlayPause->setIcon(QIcon(":/svgs/svgs/pause.svg"));
        else
            ui.btnPlayPause->setIcon(QIcon(":/svgs/svgs/play.svg"));
    }
    // 设置音量按钮为静音或非静音状态
    void setVolumeButtonState(bool isMuted, double vol = -1) {
        if (isMuted)
            ui.btnVolume->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeMuted));
        else
        {
            if (vol < 0) vol = getVolumeFromUI();
            //constexpr double fragment = 1 / 3.0;
            //int rst = vol / fragment;
            int rst = vol * 3; // 化简vol / (1 / 3.0) = vol * 3
            switch (rst)
            {
            default: // < 1.00000
            case 2: // < 0.99999
                ui.btnVolume->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeHigh));
                break;
            case 1: // < 0.66666
                ui.btnVolume->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeMedium));
                break;
            case 0: // < 0.33333
                if (vol != 0)
                    ui.btnVolume->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeLow));
                else // <= 0.00000
                    ui.btnVolume->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::AudioVolumeMuted));
                break;
            }
        }
    }

    double getVolumeFromUIValue(int value) const {
        auto min = ui.sliderVolume->minimum();
        auto max = ui.sliderVolume->maximum();
        return (value - min) / (double)(max - min);
    }
    double getVolumeFromUI() const {
        return getVolumeFromUIValue(ui.sliderVolume->value());
    }

    static double calcMsFromTimeStamp(uint64_t pts, AVRational timeBase) {
        return pts * timeBase.num / (static_cast<double>(timeBase.den) / 1000);
    }
    // 根据总时间计算总时长/ms
    static uint64_t calcMsFromAVDuration(uint64_t duration) {
        return duration / (AV_TIME_BASE / 1000);
    }
    // 根据滑块值（当前播放时间/ms）计算要seek到的时间，单位微秒
    template <typename T>
    static uint64_t calcSeekTimeFromMs(T value) {
        return static_cast<size_t>(value) * (AV_TIME_BASE / 1000);
    }
    // 根据毫秒计算时间字符串
    static QString msToQString(uint64_t milliseconds);

    static QString getElidedString(QString str, int width, QFont font);

    // 渲染第一帧的时候调用
    void beforePlaybackCallback(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx);
    // 每一帧渲染时调用
    void videoRenderCallback(const MediaPlayer::VideoFrameContext& frameCtx);
    void audioRenderCallback(const MediaPlayer::AudioFrameContext& frameCtx);
    // 播放停止后（无论什么原因）调用
    void afterPlaybackCallback();

    // 播放指定文件
    // 此函数为异步函数，调用后立即返回，播放在另一个线程中进行，且会自动停止之前的播放
    void playerPlay(QString filePath);

    void playerStop();

    // 播放播放列表中指定索引的文件
    // 此函数会自动设置播放列表中的当前播放索引
    // 不会检查索引有效性，调用前请确保索引有效
    void playerPlayByPlayListIndex(qsizetype index);

    void playerSetVolume(double volume);


};

