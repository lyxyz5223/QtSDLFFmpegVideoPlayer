#include "QtSDLFFmpegVideoPlayer.h"
#include "SDLApp.h"
#include <QTimer>
#include <QFileDialog>
#include "PlayListWidget.h"

QtSDLFFmpegVideoPlayer::QtSDLFFmpegVideoPlayer(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    ui.playListDockWidgetContents->setPlayCallback([this](const QModelIndex& index) {
        playerPlayByPlayListIndex(index.row());
        });
    ui.videoRenderWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    ui.videoRenderWidget->setAttribute(Qt::WA_PaintOnScreen, true);
    ui.videoRenderWidget->setAttribute(Qt::WA_NativeWindow, true);
    ui.videoRenderWidget->resize(size());
    ui.videoRenderWidget->show();
    sdlWidget = new SDLWidget((HWND)ui.videoRenderWidget->winId());
    sdlWidget->show();
    sdlEventTimer = new QTimer(this);
    sdlEventTimer->setInterval(0);
    sdlEventTimer->connect(sdlEventTimer, &QTimer::timeout, this, &QtSDLFFmpegVideoPlayer::sdlEventLoop);
    sdlEventTimer->start();
    connect(ui.actionOpenFile, SIGNAL(triggered()), this, SLOT(selectFileAndPlay()));
    ui.actionQuit->connect(ui.actionQuit, &QAction::triggered, [&]() {
        this->close();
        });
    QBrush ctrlBtnBkgBrush(QColor(200, 200, 200, 50));
    ui.btnPlayPause->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnPrevious->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnNext->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnStop->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnVolume->setBackgroundBrush(Qt::transparent);
    //mediaPlayer.setRenderEventCallback(std::bind(&QtSDLFFmpegVideoPlayer::renderCallback, this, std::placeholders::_1, std::placeholders::_2), MediaPlayer::VideoUserDataType{});
    //mediaPlayer.setStartEventCallback(std::bind(&QtSDLFFmpegVideoPlayer::beforePlaybackCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), MediaPlayer::VideoUserDataType{});
}

QtSDLFFmpegVideoPlayer::~QtSDLFFmpegVideoPlayer()
{

}

bool QtSDLFFmpegVideoPlayer::nativeEvent(const QByteArray & eventType, void* message, qintptr * result)
{
    return false;
}

void QtSDLFFmpegVideoPlayer::closeEvent(QCloseEvent* e)
{
    mediaPlayer.stop();
    sdlEventTimer->stop();
    sdlEventTimer->disconnect();
    sdlEventTimer->deleteLater();
    SDLApp::instance()->stopEventLoopAsync();
    QMainWindow::closeEvent(e);
}

void QtSDLFFmpegVideoPlayer::sdlEventLoop()
{
    SDLApp::instance()->runEventLoop();
}

void QtSDLFFmpegVideoPlayer::selectFileAndPlay()
{
    auto& playlist = ui.playListDockWidgetContents->playList();
    auto oldSize = playlist.size();
    ui.playListDockWidgetContents->appendFiles();
    if (!playlist.size() || playlist.size() <= oldSize) return;
    playerPlay(playlist.at(oldSize).url);
}

void QtSDLFFmpegVideoPlayer::mediaPlayPause()
{
    if (mediaPlayer.isPlaying())
        mediaPlayer.pause(); // 切换为暂停
    else
        mediaPlayer.resume(); // 切换为播放
}

void QtSDLFFmpegVideoPlayer::mediaStop()
{
    mediaPlayer.stop();
}

void QtSDLFFmpegVideoPlayer::mediaPrevious()
{
    playerPlayByPlayListIndex(ui.playListDockWidgetContents->getPreviousPlayingIndex()); // 内部会自动设置当前播放项
}

void QtSDLFFmpegVideoPlayer::mediaNext()
{
    playerPlayByPlayListIndex(ui.playListDockWidgetContents->getNextPlayingIndex()); // 内部会自动设置当前播放项
}

void QtSDLFFmpegVideoPlayer::mediaSeek()
{
    logger.info("Seeking to: {} ms, duration: {} ms", mediaSeekingTime, duration);
    mediaPlayer.seek(calcSeekTimeFromMs(mediaSeekingTime), -1);
}

void QtSDLFFmpegVideoPlayer::mediaSliderMoved(int value)
{
    isMediaSliderMoving = true;
    mediaSeekingTime = value;
}
void QtSDLFFmpegVideoPlayer::mediaSliderPressed()
{
    isMediaSliderMoving = true;
    mediaSeekingTime = ui.sliderMediaProgress->value();
}
void QtSDLFFmpegVideoPlayer::mediaSliderReleased()
{
    isMediaSliderMoving = false;
    mediaSeek();
}
void QtSDLFFmpegVideoPlayer::mediaSliderValueChanged(int value)
{
    logger.trace("Current slider time: {} ms", value);
}

void QtSDLFFmpegVideoPlayer::mediaVolumeMuteUnmute()
{

}

void QtSDLFFmpegVideoPlayer::mediaVolumeSliderMoved()
{

}

void QtSDLFFmpegVideoPlayer::beforePlaybackCallback(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx)
{
    this->duration = calcMsFromAVDuration(formatCtx->duration); // 转换为毫秒
    logger.info() << "Media Duration (ms):" << this->duration;
    ui.sliderMediaProgress->setRange(0, this->duration); // 设置滑块范围
    ui.sliderMediaProgress->setValue(0); // 重置进度条
}
void QtSDLFFmpegVideoPlayer::videoRenderCallback(const MediaPlayer::VideoFrameContext& frameCtx)
{
    // 更新进度条
    if (!isMediaSliderMoving)
    {
        int currentTime = calcMsFromTimeStamp(frameCtx.swRawFrame->pts, frameCtx.formatCtx->streams[frameCtx.streamIndex]->time_base);
        auto sliderMin = ui.sliderMediaProgress->minimum();
        ui.sliderMediaProgress->setValue(currentTime + sliderMin);
    }
}
void QtSDLFFmpegVideoPlayer::audioRenderCallback(const MediaPlayer::AudioFrameContext& frameCtx)
{
    // 更新进度条
    if (!isMediaSliderMoving)
    {
        auto sliderMin = ui.sliderMediaProgress->minimum();
        ui.sliderMediaProgress->setValue(frameCtx.frameTime * 1000 + sliderMin);
    }
}
void QtSDLFFmpegVideoPlayer::afterPlaybackCallback()
{
    // 播放结束，重置进度条
    ui.sliderMediaProgress->setValue(ui.sliderMediaProgress->minimum());
}

void QtSDLFFmpegVideoPlayer::playerPlay(QString filePath)
{
    std::thread([this, filePath]() {
        mediaPlayer.stop();
        mediaPlayer.play(filePath.toStdString(), SDLApp::getWindowId(sdlWidget->getSDLWindow()));
        logger.info() << "Media playback Finished";
        }).detach();
}

void QtSDLFFmpegVideoPlayer::playerPlayByPlayListIndex(qsizetype index)
{
    QString filePath = ui.playListDockWidgetContents->getPlayListItem(index).url;
    ui.playListDockWidgetContents->setCurrentPlayingIndex(index);
    playerPlay(filePath);
}

