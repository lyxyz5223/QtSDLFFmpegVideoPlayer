#include "SDLVideoPlayer.h"
#include "SDLApp.h"

SDLVideoPlayer::~SDLVideoPlayer()
{

}

SDLVideoPlayer::SDLVideoPlayer()
{

}

void SDLVideoPlayer::setupPlayer(SDL_WindowID winId, VideoRenderFunction renderer, UserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, UserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, UserDataType pfcUserData)
{
    this->currentWindowId = winId;
    SDLApp::runOnMainThread([&](void*) {
        this->currentWindow = SDL_GetWindowFromID(winId);
        // 获取当前窗口标题
        this->originalWindowTitle = SDL_GetWindowTitle(currentWindow);
        });
    this->currentRenderer = SDL_GetRenderer(currentWindow);
    this->renderFunction = renderer;
    this->rendererUserData = rendererUserData;
    this->beforePlaybackCallback = beforePlaybackCallback;
    this->bpcUserData = bpcUserData;
    this->playbackFinishedCallback = playbackFinishedCallback;
    this->pfcUserData = pfcUserData;
}

bool SDLVideoPlayer::playVideoWithAudio(const std::string& filePath, SDL_WindowID winId, bool autoResizeWindow, VideoRenderFunction renderer, UserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, UserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, UserDataType pfcUserData)
{
    setupPlayer(winId, renderer, rendererUserData, beforePlaybackCallback, bpcUserData, playbackFinishedCallback, pfcUserData);
    SizeI windowSize = currentWindowSizeInPixel;
    if (SDL_GetWindowSizeInPixels(currentWindow, &windowSize.w, &windowSize.h))
        currentWindowSizeInPixel = windowSize; // 如果成功，保存当前窗口大小用于缩放视频帧

    // 创建纹理智能指针
    auto bpc = [&, filePath, autoResizeWindow](const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, std::any userData) {
        // 设置窗口标题
        SDL_SetWindowTitle(currentWindow, filePath.c_str());
        // 在播放前设置窗口大小和创建纹理
        auto windowScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(currentWindow));
        // 设置窗口大小
        if (autoResizeWindow && videoCodecCtx)
        {
            SizeI logicWindowSize{ static_cast<int>(videoCodecCtx->width / windowScale), static_cast<int>(videoCodecCtx->height / windowScale) };
            SDL_SetWindowSize(currentWindow, logicWindowSize.width(), logicWindowSize.height());
            currentWindowSizeInPixel = SizeI{ videoCodecCtx->width, videoCodecCtx->height };
        }
        // 创建SDL纹理
        currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            currentWindowSizeInPixel.width(), currentWindowSizeInPixel.height()));
        //auto props = SDL_CreateProperties();
        //SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER, );
        //texture.reset(SDL_CreateTextureWithProperties(renderer, props));
        if (!currentTexture)
        {
            SDL_Log("无法创建纹理: %s", SDL_GetError());
            return;
        }
        if (this->beforePlaybackCallback)
            this->beforePlaybackCallback(formatCtx, videoCodecCtx, this->bpcUserData);
        };
    VideoRenderFunction rf = [&](const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, StreamIndexType streamIndex, AVFrame* hwVideoRawFrame, AVFrame* videoRawFrame, AVFrame* videoNewFormatFrame, bool isHardwareDecode, AVPixelFormat hwVideoFramePixelFormat, AVPixelFormat switchFormat, std::any userData) {
        // 在主线程渲染内容
        SDLApp::runOnMainThread([&](void*) {
            auto renderFrame = videoNewFormatFrame;
            if (!renderFrame)
                return;
            if (!currentTexture)
            {
                // 设置窗口标题
                SDL_SetWindowTitle(currentWindow, filePath.c_str());
                // 在播放前设置窗口大小和创建纹理
                auto windowScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(currentWindow));
                // 设置窗口大小
                if (autoResizeWindow && videoCodecCtx)
                {
                    SizeI logicWindowSize{ static_cast<int>(videoCodecCtx->width / windowScale), static_cast<int>(videoCodecCtx->height / windowScale) };
                    SDL_SetWindowSize(currentWindow, logicWindowSize.width(), logicWindowSize.height());
                    currentWindowSizeInPixel = SizeI{ videoCodecCtx->width, videoCodecCtx->height };
                }
                // 创建SDL纹理
                currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
                    SDL_TEXTUREACCESS_STREAMING,
                    currentWindowSizeInPixel.width(), currentWindowSizeInPixel.height()));
                //auto props = SDL_CreateProperties();
                //SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER, );
                //texture.reset(SDL_CreateTextureWithProperties(renderer, props));
                if (!currentTexture)
                {
                    SDL_Log("无法创建纹理: %s", SDL_GetError());
                    //logger.error("无法渲染视频帧，纹理未创建");
                    return;
                }
                if (this->beforePlaybackCallback)
                    this->beforePlaybackCallback(formatCtx, videoCodecCtx, this->bpcUserData);
                return;
            }
            // 更新纹理
            SDL_UpdateYUVTexture(currentTexture.get(), nullptr,
                renderFrame->data[0], renderFrame->linesize[0],
                renderFrame->data[1], renderFrame->linesize[1],
                renderFrame->data[2], renderFrame->linesize[2]);
            // 渲染
            SDL_RenderClear(currentRenderer);
            SDL_RenderTexture(currentRenderer, currentTexture.get(), nullptr, nullptr);
            SDL_RenderPresent(currentRenderer);
            }, 0, true/*等待执行结束*/);
        if (this->renderFunction)
            this->renderFunction(formatCtx, videoCodecCtx, streamIndex, hwVideoRawFrame, videoRawFrame, videoNewFormatFrame, isHardwareDecode, hwVideoFramePixelFormat, switchFormat, this->rendererUserData);
        };
    auto pfc = [&](std::any userData) {
        SDLApp::runOnMainThread([&](void*) {
            // 清空窗口
            SDL_SetRenderDrawColor(currentRenderer, 0, 0, 0, 255); // 全黑
            SDL_RenderClear(currentRenderer); // 绘制清除颜色：黑色
            SDL_RenderPresent(currentRenderer); // 双缓冲，需要将这一帧覆盖当前显示帧
            // 恢复窗口标题
            SDL_SetWindowTitle(currentWindow, originalWindowTitle.c_str());
            }); // 默认参数：等待执行结束
        if (this->playbackFinishedCallback)
            this->playbackFinishedCallback(this->pfcUserData);
        };
    VideoPlayer::VideoPlayOptions options;
    options.enableHardwareDecoding = true;
    options.clockSyncFunction = nullptr;
    options.renderer = rf;
    //options.rendererUserData = ;
    options.streamIndexSelector = [](StreamIndexType& outStreamIndex, MediaType type, const std::vector<StreamIndexType>& streamIndicesList, const AVFormatContext* fmtCtx, const AVCodecContext* codecCtx) {
        if (streamIndicesList.empty())
            return false;
        else
            outStreamIndex = streamIndicesList[0];
        return true;
        };
    options.frameSwitchOptions.enabled = true;
    options.frameSwitchOptions.format = AV_PIX_FMT_YUV420P;
    options.frameSwitchOptions.size = currentWindowSizeInPixel;
    return VideoPlayer::play(filePath, options);
}

