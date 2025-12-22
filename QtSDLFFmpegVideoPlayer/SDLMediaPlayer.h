#pragma once
#include "MediaPlayer.h"
#include <SDL3/SDL.h>

class SDLMediaPlayer : public MediaPlayer
{
public:
    using BeforePlaybackCallback = std::function<void(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoPlayer::UserDataType userData)>;
    using PlaybackFinishedCallback = std::function<void(VideoPlayer::UserDataType userData)>;

    // 播放带有音频的视频文件，无音频的视频可能会出现异常
    bool playVideoWithAudio(const std::string& filePath, SDL_WindowID winId, VideoRenderFunction renderer = nullptr, VideoUserDataType rendererUserData = VideoUserDataType{}, BeforePlaybackCallback beforePlaybackCallback = nullptr, VideoUserDataType bpcUserData = VideoUserDataType{}, PlaybackFinishedCallback playbackFinishedCallback = nullptr, VideoUserDataType pfcUserData = VideoUserDataType{});

    void stop() override {
        MediaPlayer::notifyStop();
        SDL_Event e;
        while (!isStopped())
        {
            SDL_PollEvent(&e);
            ThreadYield();
        }
    }

public:
    SDLMediaPlayer() {}
    ~SDLMediaPlayer() {
        if (!isStopped())
        {
            this->stop();
        }
    }

private:
    DefinePlayerLoggerSinks(loggerSinks, "SDLMediaPlayer.class.log");
    Logger logger{ "SDLMediaPlayer", loggerSinks };
    // setupPlayer设置播放器参数
    SDL_WindowID currentWindowId{ 0 };
    SDL_Window* currentWindow{ nullptr };
    SDL_Renderer* currentRenderer{ nullptr };
    std::string originalWindowTitle{ "" };

    // 非setupPlayer设置部分
    UniquePtr<SDL_Texture> currentTexture{ nullptr, [](SDL_Texture* t) { if (t) SDL_DestroyTexture(t);  } };
    SizeI prevWindowSizeInPixel;

    // setupPlayer设置播放器参数
    VideoRenderFunction renderFunction = nullptr;
    VideoUserDataType rendererUserData{};
    BeforePlaybackCallback beforePlaybackCallback = nullptr;
    VideoUserDataType bpcUserData{};
    PlaybackFinishedCallback playbackFinishedCallback = nullptr;
    VideoUserDataType pfcUserData{};

    void setupPlayer(SDL_WindowID winId, VideoRenderFunction renderer, VideoUserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, VideoUserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, VideoUserDataType pfcUserData);

};

