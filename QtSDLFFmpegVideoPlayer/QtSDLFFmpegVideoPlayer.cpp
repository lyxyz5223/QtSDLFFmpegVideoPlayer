#include "QtSDLFFmpegVideoPlayer.h"
#include "SDLApp.h"
#include <QTimer>
#include <QFileDialog>

QtSDLFFmpegVideoPlayer::QtSDLFFmpegVideoPlayer(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    ui.videoRenderWidget->setAttribute(Qt::WA_PaintOnScreen);
    ui.videoRenderWidget->setAttribute(Qt::WA_NativeWindow);
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

void QtSDLFFmpegVideoPlayer::selectFile()
{
    // 打开文件对话框
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Video File"), "", tr("Video Files (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.mp3);;All Files (*)"));
    if (!fileName.isEmpty())
    {
        qDebug() << "Selected file:" << fileName;
    }
}

void QtSDLFFmpegVideoPlayer::selectFileAndPlay()
{
    // 打开文件对话框
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Video File"), "", tr("Video Files (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.mp3);;All Files (*)"));
    if (!fileName.isEmpty())
    {
        std::thread([this, fileName]() {
            mediaPlayer.stop();
            mediaPlayer.playVideoWithAudio(fileName.toStdString(), SDLApp::getWindowId(sdlWidget->getSDLWindow()),
                [&](const MediaPlayer::VideoFrameContext& frameCtx, MediaPlayer::VideoUserDataType userData) {
                    this->renderCallback(frameCtx, userData);
                }, VideoPlayer::UserDataType{}, [&](const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, MediaPlayer::VideoUserDataType userData) {
                    this->beforePlaybackCallback(formatCtx, videoCodecCtx, userData);
                }, VideoPlayer::UserDataType{}, [&](MediaPlayer::VideoUserDataType userData) {
                    this->afterPlaybackCallback(userData);
                }, VideoPlayer::UserDataType{});
            qDebug() << "Video Playback Finished";
            }).detach();
    }
}

void QtSDLFFmpegVideoPlayer::mediaPlayPause()
{
    if (mediaPlayer.isPlaying())
    {
        // 切换为暂停
        mediaPlayer.pause();
        ui.btnPlayPause->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart));
    }
    else
    {
        // 切换为播放
        mediaPlayer.resume();
        ui.btnPlayPause->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackPause));
    }
}

void QtSDLFFmpegVideoPlayer::mediaStop()
{
    mediaPlayer.stop();
}

void QtSDLFFmpegVideoPlayer::mediaPrevious()
{

}

void QtSDLFFmpegVideoPlayer::mediaNext()
{

}

void QtSDLFFmpegVideoPlayer::mediaSeek()
{
    auto& value = mediaSliderMovingTo;
    mediaPlayer.seek(static_cast<size_t>(value) * AV_TIME_BASE, -1);
    //vp.seekAbsolute(static_cast<size_t>(value) * AV_TIME_BASE, -1, false);
    qDebug() << "Seeking to:" << value << "s";
    qDebug() << "Video Duration (s):" << this->videoDuration / AV_TIME_BASE;
}

void QtSDLFFmpegVideoPlayer::mediaSliderMoved(int value)
{
    //qDebug() << "Slider moved to:" << value;
    isMediaSliderMoving = true;
    mediaSliderMovingTo = value;
}

void QtSDLFFmpegVideoPlayer::mediaSliderPressed()
{
    isMediaSliderMoving = true;
    qDebug() << "Slider pressed at:" << ui.sliderMediaProgress->value();
    mediaSliderMovingTo = ui.sliderMediaProgress->value();
}

void QtSDLFFmpegVideoPlayer::mediaSliderReleased()
{
    qDebug() << "Slider released at:" << mediaSliderMovingTo;
    isMediaSliderMoving = false;
    mediaSeek();
}

void QtSDLFFmpegVideoPlayer::mediaSliderValueChanged(int value)
{
    qDebug() << "Slider value changed to:" << value;
}

void QtSDLFFmpegVideoPlayer::beforePlaybackCallback(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoPlayer::UserDataType userData)
{
    //if (videoCodecCtxMap.count(videoStreamIndex))
    //{
    //    auto& codecCtx = videoCodecCtxMap.at(videoStreamIndex);
    //}
    //else if (audioCodecCtxMap.count(audioStreamIndex))
    //{
    //    auto& codecCtx = audioCodecCtxMap.at(audioStreamIndex);
    //}
    this->videoDuration = formatCtx->duration;
    ui.sliderMediaProgress->setRange(0, static_cast<int>(this->videoDuration / AV_TIME_BASE));
    qDebug() << "Video Duration (s):" << this->videoDuration / AV_TIME_BASE;
    // 更新进度条
    ui.sliderMediaProgress->setValue(ui.sliderMediaProgress->minimum());
    // 将播放按钮设置为播放中
    ui.btnPlayPause->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackPause));
}

void QtSDLFFmpegVideoPlayer::renderCallback(const MediaPlayer::VideoFrameContext& frameCtx, MediaPlayer::VideoUserDataType userData)
{
    // 更新进度条
    if (!isMediaSliderMoving)
    {
        int currentTime = static_cast<int>(frameCtx.swRawFrame->pts * av_q2d(frameCtx.formatCtx->streams[frameCtx.streamIndex]->time_base));
        ui.sliderMediaProgress->setValue(currentTime + ui.sliderMediaProgress->minimum());
    }
}

void QtSDLFFmpegVideoPlayer::afterPlaybackCallback(VideoPlayer::UserDataType userData)
{
    // 播放结束
    ui.sliderMediaProgress->setValue(ui.sliderMediaProgress->minimum());
}

