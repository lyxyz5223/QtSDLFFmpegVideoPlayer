#include "QtSDLFFmpegVideoPlayer.h"
#include <QTimer>
#include <QTime>
#include <QFileDialog>
#include "PlayListWidget.h"
#include <QThread>
#include "PlayerOptionsWidget.h"
#ifdef USE_SDL_WIDGET
#include "SDLApp.h"
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
#include <QMediaPlayer>
#else
#endif


QtSDLFFmpegVideoPlayer::QtSDLFFmpegVideoPlayer(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // 设置视频渲染控件属性
    ui.videoRenderWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    ui.videoRenderWidget->setAttribute(Qt::WA_PaintOnScreen, true);
    ui.videoRenderWidget->setAttribute(Qt::WA_NativeWindow, true);
    ui.videoRenderWidget->show();
    // 创建SDL窗口和渲染器
    createVideoWidget();
    taskbarMediaController.initialize(reinterpret_cast<HWND>(this->winId())); // 初始化任务栏媒体控制器

    // 启动SDL事件循环定时器，单次触发，间隔0ms
    sdlEventTimer = new QTimer(this);
    sdlEventTimer->setSingleShot(true);
    sdlEventTimer->setInterval(0);
    sdlEventTimer->connect(sdlEventTimer, &QTimer::timeout, this, &QtSDLFFmpegVideoPlayer::sdlEventLoop);
    sdlEventTimer->start();

    // 绑定菜单列表
    connect(ui.actionOpenFiles, SIGNAL(triggered()), this, SLOT(selectFilesAndPlay()));
    connect(ui.actionOpenFolder, SIGNAL(triggered()), this, SLOT(selectFolderAndPlay()));
    connect(ui.actionOpenUrl, SIGNAL(triggered()), this, SLOT(selectUrlAndPlay()));
    connect(ui.actionMenuOpenActionOpenFiles, SIGNAL(triggered()), this, SLOT(selectFilesAndPlay()));
    connect(ui.actionMenuOpenActionOpenFolder, SIGNAL(triggered()), this, SLOT(selectFolderAndPlay()));
    connect(ui.actionMenuOpenActionOpenUrl, SIGNAL(triggered()), this, SLOT(selectUrlAndPlay()));
    connect(ui.actionSettings, &QAction::triggered, [&]() { this->openSettingsDialog(); });
    connect(ui.actionQuit, &QAction::triggered, [&]() { this->close(); });
    // 绑定控制按钮、音量按钮和滑块
    QBrush ctrlBtnBkgBrush(QColor(200, 200, 200, 50));
    ui.btnPlayPause->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnPrevious->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnNext->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnStop->setBackgroundBrush(ctrlBtnBkgBrush);
    ui.btnVolume->setBackgroundBrush(Qt::transparent);
    // 初始化播放器音频音量
    playerSetVolume(getVolumeFromUI());

    // 播放列表
    // 绑定播放器控制按钮槽函数
    ui.playListDockWidgetContents->connect(ui.playListDockWidgetContents, &PlayListWidget::play, this, &QtSDLFFmpegVideoPlayer::playerPlayByPlayListIndex);

    optionsWidget = new PlayerOptionsWidget(this);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::stepBackLong, [&]() { playerStepBack(PLAYER_STEP_LONG_MS); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::stepBackShort, [&]() { playerStepBack(PLAYER_STEP_SHORT_MS); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::stepForwardLong, [&]() { playerStepForward(PLAYER_STEP_LONG_MS); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::stepForwardShort, this, [&]() { playerStepForward(PLAYER_STEP_SHORT_MS); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::playSpeedChange, this, &QtSDLFFmpegVideoPlayer::playerSetSpeed);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::playSpeedReset, this, [&]() { playerSetSpeed(1.0); });
    //optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::playerABLoopIntervalSideASet, this, &QtSDLFFmpegVideoPlayer::);
    //optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::playerABLoopIntervalSideBSet, this, &QtSDLFFmpegVideoPlayer::);
    //optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::playerABLoopIntervalRemove, this, &QtSDLFFmpegVideoPlayer::);

    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoBrightnessChange, this, &QtSDLFFmpegVideoPlayer::playerSetBrightness);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoContrastChange, this, &QtSDLFFmpegVideoPlayer::playerSetContrast);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoSaturationChange, this, &QtSDLFFmpegVideoPlayer::playerSetSaturation);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoChromaticityChange, this, &QtSDLFFmpegVideoPlayer::playerSetChromaticity);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoBrightnessReset, this, [&] { playerSetBrightness(0); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoContrastReset, this, [&] { playerSetContrast(0); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoSaturationReset, this, [&] { playerSetSaturation(0); });
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::videoChromaticityReset, this, [&] { playerSetChromaticity(0); });

    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::equalizerEnableStateChange, this, &QtSDLFFmpegVideoPlayer::playerSetEqualizerState);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::equalizerGainChange, this, &QtSDLFFmpegVideoPlayer::playerSetEqualizerGain);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::equalizerGainsChange, this, &QtSDLFFmpegVideoPlayer::playerSetEqualizerGains);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::systemMixerVolumeChange, this, &QtSDLFFmpegVideoPlayer::systemMixerSetVolume);
    optionsWidget->connect(optionsWidget, &PlayerOptionsWidget::systemVolumeChange, this, &QtSDLFFmpegVideoPlayer::systemSetVolume);


    optionsWidget->setPlaySpeed(mediaPlayer.getSpeed());
    optionsWidget->setEqualizerEnabled(mediaPlayer.getEqualizerState());

    systemVolumeController.setVolumeChangeCallback([this] (ISystemVolumeController::VolumeDeviceType type, float volume, bool muted) {
        if (type == SystemVolumeController::Master)
        {
            optionsWidget->setSystemVolumeUI(volume);
        }
        else if (type == SystemVolumeController::Mixer)
        {
            optionsWidget->setSystemMixerVolumeUI(volume); 
        }
        });
    auto initVol = systemVolumeController.getSystemMasterVolume();
    optionsWidget->setSystemVolumeUI(initVol);

    videoPreviewThumbnailWidget = new QLabel(this);
    videoPreviewThumbnailWidget->setWindowFlag(Qt::FramelessWindowHint, true);
    videoPreviewThumbnailWidget->setAttribute(Qt::WA_ShowWithoutActivating, true);
    videoPreviewThumbnailWidget->setFocusPolicy(Qt::NoFocus);
    // 创建调色板
    //QPalette palette;
    //palette.setBrush(videoPreviewThumbnailWidget->backgroundRole(), QColor(255, 255, 255, 255));
    // 应用调色板
    //videoPreviewThumbnailWidget->setAutoFillBackground(true);
    //videoPreviewThumbnailWidget->setPalette(palette);
    videoPreviewThumbnailWidget->setScaledContents(true);
    videoPreviewThumbnailWidget->installEventFilter(this);
    ui.sliderMediaProgress->installEventFilter(this);
    show();
    taskbarMediaController.addThumbBarButtons();
}

QtSDLFFmpegVideoPlayer::~QtSDLFFmpegVideoPlayer()
{
    destroyVideoWidget();
}

bool QtSDLFFmpegVideoPlayer::nativeEvent(const QByteArray & eventType, void* message, qintptr * result)
{
    return false;
}

void QtSDLFFmpegVideoPlayer::closeEvent(QCloseEvent* e)
{
    playerStop();
    sdlEventTimer->stop();
    sdlEventTimer->disconnect();
    sdlEventTimer->deleteLater();
#ifdef USE_SDL_WIDGET
    //SDLApp::instance()->stopEventLoopAsync();
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
#else
#endif
    QMainWindow::closeEvent(e);
}

void QtSDLFFmpegVideoPlayer::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    auto idx = ui.playListDockWidgetContents->getCurrentPlayingIndex();
    if (idx < 0 || idx >= ui.playListDockWidgetContents->getPlayListSize()) return;
    QString elidedMediaName = getElidedString(ui.playListDockWidgetContents->getPlayListItem(idx).title, ui.labelMediaName->width(), ui.labelMediaName->font()); // 设置媒体名称显示
    ui.labelMediaName->setText(elidedMediaName);
}

bool QtSDLFFmpegVideoPlayer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == nullptr)
    {
        return false;
    }
    else if (watched == ui.sliderMediaProgress)
    {
        switch (event->type())
        {
        case QEvent::Enter:
        case QEvent::MouseMove:
        {
            if (!videoPreviewThumbnailWidget)
                break;
            std::unique_lock lock(mtxPreviewDemuxer);
            if (!previewDemuxer->isOpen() || !previewCodecCtx.get())
                break;
            QRect videoPreviewThumbnailWidgetRect{ mapFromGlobal(QCursor::pos()).x(), ui.sliderMediaProgress->mapTo(this, ui.sliderMediaProgress->rect().topLeft()).y(), 160, 90 };
            videoPreviewThumbnailWidget->resize(videoPreviewThumbnailWidgetRect.size());
            videoPreviewThumbnailWidget->move(
                videoPreviewThumbnailWidgetRect.x() - videoPreviewThumbnailWidgetRect.size().width() / 2,
                videoPreviewThumbnailWidgetRect.y() - videoPreviewThumbnailWidgetRect.size().height() - 5
            );
            videoPreviewThumbnailWidget->show();
            uint64_t seekToTimeMs = calcSliderValueFromGlobalPos(ui.sliderMediaProgress, QCursor::pos());
            if (seekToTimeMs / 1000 == previewCurrentTimeMs / 1000)
                break;
            if (seekToTimeMs > previewCurrentTimeMs)
            {
                if (seekToTimeMs / 1000 - previewCurrentTimeMs / 1000 < 5)
                    break;
            }
            else if (seekToTimeMs < previewCurrentTimeMs)
            {
                if (previewCurrentTimeMs / 1000 - seekToTimeMs / 1000 < 5)
                    break;
            }
            logger.info("Preview thumbnail seek time: {} ms, time string: {}", seekToTimeMs, msToQString(seekToTimeMs).toStdString());
            previewCurrentTimeMs = seekToTimeMs;
            auto seekToPts = calcSeekTimeFromMs(seekToTimeMs);
            if (av_seek_frame(previewDemuxer->getFormatContext(), -1/*previewDemuxer->getStreamIndex()*/, seekToPts, AVSEEK_FLAG_BACKWARD) < 0) // 定位到指定时间戳附近的关键帧
                break;
            previewDemuxer->flushPacketQueue(); // 定位后需要刷新packet队列，否则可能会读到之前的packet
            avcodec_flush_buffers(previewCodecCtx.get());
            AVPacket* pkt = nullptr;
            AbstractPlayer::UniquePtr<AVFrame> frame{ AbstractPlayer::makeUniqueFrame() };
            while (previewDemuxer->readOnePacket(&pkt) && pkt) // 预读一帧，获取视频流信息，准备生成预览缩略图
            {
                int aspRst = avcodec_send_packet(previewCodecCtx.get(), pkt);
                if (aspRst < 0 && aspRst != AVERROR(EAGAIN))
                {
                    pkt = nullptr;
                    break;
                }
                int rst = avcodec_receive_frame(previewCodecCtx.get(), frame.get());
                if (rst >= 0)
                    break;
                if (rst != AVERROR(EAGAIN))
                {
                    pkt = nullptr;
                    break;
                }
            }
            if (!pkt) break;
            logger.trace("Preview current frame pts: {}, duration: {}", frame->pts, previewDemuxer->getFormatContext()->streams[previewDemuxer->getStreamIndex()]->duration);
            QSize scaleSize{ frame->width, frame->height };
            QImage image{ scaleSize.width(), scaleSize.height(), QImage::Format_RGB888};
            uint8_t* imageData[4] = { image.bits(), nullptr, nullptr, nullptr };
            int imageLinesize[4] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
            // sws转换插值算法
            if (!previewSwsCtx)
            {
                constexpr SwsFlags swsFlags = SWS_BILINEAR;
                previewSwsCtx.reset(sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), scaleSize.width(), scaleSize.height(), AV_PIX_FMT_RGB24, swsFlags, 0, 0, 0));
                if (!previewSwsCtx)
                    break;
            }
            // 转换像素格式
            int outputSliceHeight = sws_scale(previewSwsCtx.get(), (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, imageData, imageLinesize);
            if (outputSliceHeight < 0)
                break;
            videoPreviewThumbnailWidget->setPixmap(QPixmap::fromImage(image));
            //videoPreviewThumbnailWidget->setText(msToQString(ui.sliderMediaProgress->value() - ui.sliderMediaProgress->minimum()));
        }
            break;
        case QEvent::Leave:
        {
            if (videoPreviewThumbnailWidget)
                videoPreviewThumbnailWidget->hide();
            break;
        }
        default:
            break;
        }
    }
    else if (watched == videoPreviewThumbnailWidget)
    {
        switch (event->type())
        {
        case QEvent::Paint:
        {
            //QPainter p(videoPreviewThumbnailWidget);
            //p.setPen(QColor(255, 127, 80, 255));
            //p.setBrush(Qt::NoBrush);
            //p.drawRoundedRect(videoPreviewThumbnailWidget->rect().adjusted(0, 0, -1, -1), 5, 5);
            break;
        }
        default:
            break;
        }
    }
    return false;
}

void QtSDLFFmpegVideoPlayer::sdlEventLoop()
{
#ifdef USE_SDL_WIDGET
    SDLApp::instance()->runEventLoop();
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
#else
#endif
}

void QtSDLFFmpegVideoPlayer::selectFilesAndPlay()
{
    auto& playlist = ui.playListDockWidgetContents->playList();
    auto oldSize = playlist.size();
    ui.playListDockWidgetContents->appendFiles();
    if (!playlist.size() || playlist.size() <= oldSize) return;
    playerPlayByPlayListIndex(oldSize);
}

void QtSDLFFmpegVideoPlayer::selectFolderAndPlay()
{

}

void QtSDLFFmpegVideoPlayer::selectUrlAndPlay()
{

}

void QtSDLFFmpegVideoPlayer::openSettingsDialog()
{
    // 打开设置窗口（注：不是播放选项窗口，打开播放选项窗口是mediaOpenPlayOptionsDialog）
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
    playerStop();
}

void QtSDLFFmpegVideoPlayer::mediaPrevious()
{
    playerPlayByPlayListIndex(ui.playListDockWidgetContents->getPreviousPlayingIndex()); // 内部会自动设置当前播放项
}

void QtSDLFFmpegVideoPlayer::mediaNext()
{
    playerPlayByPlayListIndex(ui.playListDockWidgetContents->getNextPlayingIndex()); // 内部会自动设置当前播放项
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
    playerSeekTo(mediaSeekingTime);
}
void QtSDLFFmpegVideoPlayer::mediaSliderValueChanged(int value)
{
    logger.trace("Current slider time: {} ms", value);
}

void QtSDLFFmpegVideoPlayer::mediaVolumeMuteUnmute()
{
    bool isMute = mediaPlayer.getMute();
    mediaPlayer.setMute(!isMute);
    setVolumeButtonState(!isMute);
}

void QtSDLFFmpegVideoPlayer::mediaVolumeSliderMoved(int value)
{
    mediaVolumeSliderValueChanged(value);
}

void QtSDLFFmpegVideoPlayer::mediaVolumeSliderValueChanged(int value)
{
    double vol = getVolumeFromUIValue(value);
    playerSetVolume(vol);
    setVolumeButtonState(vol <= 0, vol);
}

void QtSDLFFmpegVideoPlayer::mediaOpenPlayOptionsDialog()
{
    optionsWidget->show();
}

QString QtSDLFFmpegVideoPlayer::msToQString(uint64_t milliseconds)
{
    uint64_t totalSeconds = milliseconds / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    QTime time{ hours, minutes, seconds };
    return time.toString("HH:mm:ss");
}

QString QtSDLFFmpegVideoPlayer::getElidedString(QString str, int width, QFont font)
{
    QFontMetrics fm(font);
    QString rst = fm.elidedText(str, Qt::ElideRight, width);
    return rst;
}

void QtSDLFFmpegVideoPlayer::beforePlaybackCallback(AbstractPlayer::StreamType streamType, const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, AbstractPlayer::StreamIndexType streamIndex)
{
    auto dur = calcMsFromAVDuration(formatCtx->streams[streamIndex]->duration, formatCtx->streams[streamIndex]->time_base); // 转换为毫秒;;
    if (this->duration < dur || (this->duration == dur && dur != 0 && streamType & AbstractPlayer::StreamType::STAudio))
    {// 时长长的优先，其次是音频优先
        this->timeUpdateStream = streamType;
        this->duration = dur;
    }
    else if (this->duration == 0) 
    {
        this->timeUpdateStream = streamType;
        this->duration = calcMsFromAVDuration(formatCtx->duration); // 转换为毫秒
    }
    if (streamType & AbstractPlayer::STVideo)
    {
        std::unique_lock lock(mtxPreviewDemuxer);
        previewDemuxer->selectStreamIndex([streamIndex](AbstractPlayer::StreamIndexType& outStreamIndex, AbstractPlayer::StreamType type, const std::span<AVStream*> streamIndexesList, const AVFormatContext* fmtCtx) -> bool {
                if (streamIndex == -1)
                    return false;
                outStreamIndex = streamIndex;
                return true; // 选择第一个视频流
            });
        if (!MediaDecodeUtils::findAndOpenVideoDecoder(&logger, previewDemuxer->getFormatContext(), previewDemuxer->getStreamIndex(), previewCodecCtx, false/*hwEnabled*/))
            previewCodecCtx.reset(nullptr);
    }
    logger.info() << "Media Duration (ms):" << this->duration;
    QString elidedMediaName = getElidedString(ui.playListDockWidgetContents->getPlayListItem(ui.playListDockWidgetContents->getCurrentPlayingIndex()).title, ui.labelMediaName->width(), ui.labelMediaName->font()); // 设置媒体名称显示
    auto updateUI = [this, elidedMediaName] {
        ui.sliderMediaProgress->setRange(0, this->duration); // 设置滑块范围
        ui.sliderMediaProgress->setValue(0); // 重置进度条
        ui.labelMediaCurrentTime->setText(msToQString(0));// 重置当前时间显示
        ui.labelMediaDuration->setText(msToQString(this->duration));// 设置总时间显示
        ui.labelMediaName->setText(elidedMediaName);
    };
    if (QThread::currentThread() == QApplication::instance()->thread())
        updateUI();
    else
        QMetaObject::invokeMethod(this, updateUI/*, Qt::QueuedConnection*/);
}
void QtSDLFFmpegVideoPlayer::videoRenderCallback(const MediaPlayer::VideoFrameContext& frameCtx)
{
    if (timeUpdateStream != AbstractPlayer::StreamType::STVideo)
        return;
    uint64_t currentTime = calcMsFromTimeStamp(frameCtx.swRawFrame->pts + frameCtx.swRawFrame->duration, frameCtx.formatCtx->streams[frameCtx.streamIndex]->time_base);
    uint64_t curTimeS = currentTime / 1000;
    this->currentTimeMs = currentTime;
    if (this->currentTimeS == curTimeS || isMediaSliderMoving)
        return; // 秒级别未变化则不更新UI，或者用户正在拖动进度条则不更新UI
    this->currentTimeS = curTimeS;
    auto updateUI = [this, currentTime] {
        QString strTime = msToQString(currentTime);
        ui.labelMediaCurrentTime->setText(strTime); // 设置数字时间进度
        auto sliderMin = ui.sliderMediaProgress->minimum();
        ui.sliderMediaProgress->setValue(currentTime + sliderMin); // 更新进度条
    };
    if (QThread::currentThread() == QApplication::instance()->thread())
        updateUI();
    else
        QMetaObject::invokeMethod(this, updateUI/*, Qt::QueuedConnection*/);
}
void QtSDLFFmpegVideoPlayer::audioRenderCallback(const MediaPlayer::AudioFrameContext& frameCtx)
{
    if (timeUpdateStream != AbstractPlayer::StreamType::STAudio)
        return;
    uint64_t currentTime = frameCtx.frameTime * 1000 + (frameCtx.sampleRate > 0 ? (static_cast<double>(frameCtx.nFrames) / frameCtx.sampleRate * mediaPlayer.getSpeed()) : 0);
    uint64_t curTimeS = currentTime / 1000;
    this->currentTimeMs = currentTime;
    //logger.info("Update current time: {}ms", currentTime);
    if (this->currentTimeS == curTimeS || isMediaSliderMoving)
        return; // 秒级别未变化则不更新UI，或者用户正在拖动进度条则不更新UI
    this->currentTimeS = curTimeS;
    auto updateUI = [this, currentTime] {
        QString strTime = msToQString(currentTime);
        ui.labelMediaCurrentTime->setText(strTime); // 设置数字时间进度
        auto sliderMin = ui.sliderMediaProgress->minimum();
        ui.sliderMediaProgress->setValue(currentTime + sliderMin); // 更新进度条
    };
    if (QThread::currentThread() == QApplication::instance()->thread())
        updateUI();
    else
        QMetaObject::invokeMethod(this, updateUI/*, Qt::QueuedConnection*/);
}
void QtSDLFFmpegVideoPlayer::afterPlaybackCallback()
{
    // 播放结束，重置进度条
    auto updateUI = [this] {
        ui.sliderMediaProgress->setValue(ui.sliderMediaProgress->minimum());
    };
    if (QThread::currentThread() == QApplication::instance()->thread())
        updateUI();
    else
        QMetaObject::invokeMethod(this, updateUI, Qt::QueuedConnection);

}

void QtSDLFFmpegVideoPlayer::playerPlay(QString filePath)
{
    std::thread([this, filePath]() {
        mediaPlayer.stop();
        resetMediaPlayerStates();
#ifdef USE_SDL_WIDGET
        mediaPlayer.play(filePath.toStdString(), SDLApp::getWindowId(videoWidget->getSDLWindow()), true);
#elif defined(USE_QT_MULTIMEDIA_WIDGET)
        std::unique_lock lock(mtxPreviewDemuxer);
        previewDemuxer->close();
        previewDemuxer->open(filePath.toStdString());
        previewDemuxer->findStreamInfo();
        previewSwsCtx.reset(nullptr);
        lock.unlock();
        mediaPlayer.play(filePath.toStdString(), videoWidget, true);
        //previewVideoCapture.open(filePath.toStdString());
        //qMediaPlayer->stop();
        //qMediaPlayer->setSource(QUrl::fromLocalFile(filePath));
        //qMediaPlayer->play();
#else
#endif
        logger.info() << "Media playback Finished";
        }).detach();
}

void QtSDLFFmpegVideoPlayer::playerStop()
{
    ui.playListDockWidgetContents->setCurrentPlayingIndex(-1); // 清除播放状态
    mediaPlayer.stop();
    std::unique_lock lock(mtxPreviewDemuxer);
    previewDemuxer->close();
    previewDemuxer->reset();
    previewDemuxer->setStreamType(AbstractPlayer::StreamType::STVideo);
}

void QtSDLFFmpegVideoPlayer::playerPlayByPlayListIndex(qsizetype index)
{
    if (!ui.playListDockWidgetContents->getPlayListSize())
        return; // 判断播放列表是否为空，为空则不播放
    QString filePath = ui.playListDockWidgetContents->getPlayListItem(index).url;
    ui.playListDockWidgetContents->setCurrentPlayingIndex(index);
    playerPlay(filePath);
}

void QtSDLFFmpegVideoPlayer::playerSetVolume(double volume)
{
    mediaPlayer.setVolume(volume);
    mediaPlayer.setMute(volume <= 0);
}

void QtSDLFFmpegVideoPlayer::playerSetSpeed(double speed)
{
    mediaPlayer.setSpeed(speed);
}

void QtSDLFFmpegVideoPlayer::playerSeekTo(uint64_t milliseconds)
{
    logger.info("Seeking to: {} ms, duration: {} ms", milliseconds, this->duration.load());
    mediaPlayer.seek(calcSeekTimeFromMs(milliseconds), -1);
}

void QtSDLFFmpegVideoPlayer::playerStepBack(uint64_t milliseconds)
{
    auto curTime = this->currentTimeMs.load();
    if (curTime < milliseconds)
        curTime = milliseconds;
    playerSeekTo(curTime - milliseconds);
}

void QtSDLFFmpegVideoPlayer::playerStepForward(uint64_t milliseconds)
{
    auto curTime = this->currentTimeMs.load();
    auto dur = this->duration.load();
    auto seekTo = curTime + milliseconds;
    if (seekTo > dur)
        seekTo = dur;
    playerSeekTo(seekTo);
}

void QtSDLFFmpegVideoPlayer::systemSetVolume(int value)
{
#ifdef _WIN32
    double volume = getVolumeFromUIValue(value);
    systemVolumeController.setSystemMasterVolume(volume);
#else
#error "System volume control is only implemented for Windows. You can implement it for other platforms using their respective APIs."
#endif
}

void QtSDLFFmpegVideoPlayer::systemMixerSetVolume(int value)
{
#ifdef _WIN32
    double volume = getVolumeFromUIValue(value);
    systemVolumeController.setSystemMixerVolume(volume);
#else
#error "System volume control is only implemented for Windows. You can implement it for other platforms using their respective APIs."
#endif
}

