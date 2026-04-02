#pragma once
#include <MediaPlayer.h>
#include <SDL3/SDL.h>

class VideoFrameProcessor;

class SDLMediaPlayer : public MediaPlayer
{
public:
    using StartEventCallback = std::function<void(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoUserDataType userData)>;
    using StopEventCallback = std::function<void(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoUserDataType userData)>;

    enum class ScalingMode {
        StretchToFill = 0, // 拉伸填充整个窗口，可能会变形
        ScaleToFit = 1, // 等比缩放以适应窗口大小，可能有黑边
        ScaleToFill = 2, // 填充整个窗口，可能会裁剪部分画面
    };

    enum class TexturePosition {
        Center = 0,
        TopLeft = 1,
        TopRight = 2,
        BottomLeft = 3,
        BottomRight = 4,
    };

public:
    SDLMediaPlayer() {}
    ~SDLMediaPlayer() {
        if (!isStopped())
            this->stop();
    }
public:

    bool play(const std::string& filePath, SDL_WindowID winId, bool enableHardwareDecoding);
    bool play(const std::string& filePath, SDL_WindowID winId, VideoDecodeType videoDecodeType = VideoDecodeType::Unset);

    void stop() override {
        MediaPlayer::notifyStop();
        SDL_Event e;
        while (!isStopped())
        {
            SDL_PollEvent(&e);
            ThreadYield();
        }
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
    DefinePlayerLoggerSinks(loggerSinks, "SDLMediaPlayer");
    Logger logger{ "SDLMediaPlayer", loggerSinks };
    // setupPlayer设置播放器参数
    SDL_WindowID currentWindowId{ 0 };
    SDL_Window* currentWindow{ nullptr };
    SDL_Renderer* currentRenderer{ nullptr };

    // 非setupPlayer设置部分
    UniquePtr<SDL_Texture> currentTexture{ nullptr, [](SDL_Texture* t) { if (t) SDL_DestroyTexture(t);  } };
    SizeI prevWindowSizeInPixel;

    ScalingMode scalingMode{ ScalingMode::ScaleToFit };
    TexturePosition texturePosition{ TexturePosition::Center };


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
    void setupPlayer();

    void renderVideoFrame(const VideoDecodedFrameContext& frameCtx, VideoUserDataType userData);

    void cleanupPlayer();


    template <typename SizeBaseType>
    Size<SizeBaseType>& calculateTextureSize(Size<SizeBaseType>& target, const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, ScalingMode mode = ScalingMode::StretchToFill);

    template <typename SizeBaseType>
    SDL_FRect calculateTextureRectF(const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, TexturePosition position = TexturePosition::Center);


};

