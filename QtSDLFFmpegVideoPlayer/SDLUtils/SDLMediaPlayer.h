#pragma once
#include <MediaPlayer.h>
#include <SDL3/SDL.h>

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

private:
    void setupPlayer();

    void renderVideoFrame(const VideoFrameContext& frameCtx, VideoUserDataType userData);

    void cleanupPlayer();

    void frameSwitchOptionsCallback(VideoFrameSwitchOptions& to, const VideoFrameContext& frameCtx, VideoUserDataType userData);

    template <typename SizeBaseType>
    Size<SizeBaseType>& calculateTextureSize(Size<SizeBaseType>& target, const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, ScalingMode mode = ScalingMode::StretchToFill);

    template <typename SizeBaseType>
    SDL_FRect calculateTextureRectF(const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, TexturePosition position = TexturePosition::Center);
};

