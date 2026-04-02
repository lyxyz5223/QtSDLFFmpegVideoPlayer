#pragma once
#include <MediaPlayer.h>
#include <QVideoWidget>
#include <QVideoFrameFormat>
#include <FrameProcessor.h>

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

    void setEqualizerEnabled(bool enabled) {
        isEqualizerEnabled = enabled;
    }
    bool getEqualizerEnabled() const {
        return isEqualizerEnabled;
    }

    void setEqualizerGain(uint64_t bandIndex, IFFmpegFrameAudioEqualizerFilter::BandInfo gain) {
        if (bandIndex >= equalizerBandGains.size())
            return;
        equalizerBandGains[bandIndex] = gain;
    }

    double getSpeed() const {
        return speed.load();
    }
    void setSpeed(double sp) {
        speed = sp;
    }

    double getVolume() const {
        return volume.load();
    }
    void setVolume(double vol) {
        volume = vol;
    }
    bool getMute() const {
        return isMute.load();
    }
    void setMute(bool state) {
        isMute = state;
    }

    void setBrightness(float value) {
        brightness = value;
    }

    void setContrast(float value) {
        contrast = value;
    }

    void setSaturation(float value) {
        saturation = value;
    }

    void setHue(float value) {
        hue = value;
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

    // 滤镜相关参数
    Atomic<double> speed{ 1.0 };
    Atomic<double> volume{ 1.0 };
    Atomic<bool> isMute{ false };
    Atomic<bool> isEqualizerEnabled{ false };
    bool lastEqualizerEnabledState = isEqualizerEnabled.load();
    std::vector<IFFmpegFrameAudioEqualizerFilter::BandInfo> equalizerBandGains{ FFmpegFrameAudio10BandEqualizerFilter::defaultBandGains() };
    
    Atomic<float> brightness{ 0.0f };
    Atomic<float> contrast{ 0.0f };
    Atomic<float> saturation{ 1.0f };
    Atomic<float> hue{ 0.0f };

    // 渲染相关参数
    struct VideoRenderUserData {
        SharedPtr<VideoFrameProcessor> processor;
    };

private:
    std::optional<QVideoFrameFormat::PixelFormat> avPixelFormatToQVideoFrameFormat(AVPixelFormat pixelFormat);
    

    SharedPtr<QVideoFrame> createVideoFrameFromAVFrame(AVFrame* avFrame);

    void setupPlayer();

    void renderVideoFrame(const VideoDecodedFrameContext& frameCtx, VideoUserDataType userData);

    void cleanupPlayer();


};

