#pragma once
#include <MediaPlayer.h>
#include <QVideoWidget.h>
#include <QVideoFrameFormat>

class QtMultiMediaPlayer : public MediaPlayer
{
public:

public:
    QtMultiMediaPlayer() {}
    ~QtMultiMediaPlayer() {
        if (!isStopped())
            this->stop();
    }
public:

    bool play(const std::string& filePath, QVideoWidget* widget, bool enableHardwareDecoding);
    bool play(const std::string& filePath, QVideoWidget* widget, VideoDecodeType videoDecodeType = VideoDecodeType::Unset);

    static SizeI qSizeToSizeI(const QSize& size) {
        return SizeI{ size.width(), size.height() };
    }

protected:
    // 重写事件处理函数
    void startEvent(MediaPlaybackStateChangeEvent* e) override;
    void stopEvent(MediaPlaybackStateChangeEvent* e) override;


private:
    DefinePlayerLoggerSinks(loggerSinks, "QtMediaPlayer");
    Logger logger{ "QtMediaPlayer", loggerSinks };
    // setupPlayer设置播放器参数
    QVideoWidget* currentWidget{ 0 };

private:
    std::optional<QVideoFrameFormat::PixelFormat> avPixelFormatToQVideoFrameFormat(AVPixelFormat pixelFormat);
    

    QVideoFrame createVideoFrameFromAVFrame(AVFrame* avFrame);

    void setupPlayer();

    void renderVideoFrame(const VideoFrameContext& frameCtx, VideoUserDataType userData);

    void cleanupPlayer();

    void frameSwitchOptionsCallback(VideoFrameSwitchOptions& to, const VideoFrameContext& frameCtx, VideoUserDataType userData);

};

