#pragma once
#include <VideoPlayer.h>
#include <string>
#include <SDL3/SDL.h>

class SDLVideoPlayer : public VideoPlayer
{
public:
    ~SDLVideoPlayer();
    SDLVideoPlayer();

    using BeforePlaybackCallback = std::function<void(const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, VideoPlayer::UserDataType userData)>;
    using PlaybackFinishedCallback = std::function<void(VideoPlayer::UserDataType userData)>;

    // 播放带有音频的视频文件，无音频的视频可能会出现异常
    bool playVideoWithAudio(const std::string& filePath, SDL_WindowID winId, bool autoResizeWindow = false, VideoRenderFunction renderer = nullptr, UserDataType rendererUserData = UserDataType{}, BeforePlaybackCallback beforePlaybackCallback = nullptr, UserDataType bpcUserData = UserDataType{}, PlaybackFinishedCallback playbackFinishedCallback = nullptr, UserDataType pfcUserData = UserDataType{});

private:
    // setupPlayer设置播放器参数
    SDL_WindowID currentWindowId{ 0 };
    SDL_Window* currentWindow{ nullptr };
    SDL_Renderer* currentRenderer{ nullptr };
    std::string originalWindowTitle{ "" };

    // 非setupPlayer设置部分
    UniquePtr<SDL_Texture> currentTexture{ nullptr, [](SDL_Texture* t) { if (t) SDL_DestroyTexture(t);  } };
    SizeI currentWindowSizeInPixel;

    // setupPlayer设置播放器参数
    VideoRenderFunction renderFunction = nullptr;
    UserDataType rendererUserData = UserDataType{};
    BeforePlaybackCallback beforePlaybackCallback = nullptr;
    UserDataType bpcUserData = UserDataType{};
    PlaybackFinishedCallback playbackFinishedCallback = nullptr;
    UserDataType pfcUserData = UserDataType{};

    void setupPlayer(SDL_WindowID winId, VideoRenderFunction renderer, UserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, UserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, UserDataType pfcUserData);
};

