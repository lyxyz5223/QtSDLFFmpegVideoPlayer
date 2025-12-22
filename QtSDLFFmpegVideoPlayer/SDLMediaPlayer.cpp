#include "SDLMediaPlayer.h"
#include "SDLApp.h"

void SDLMediaPlayer::setupPlayer(SDL_WindowID winId, VideoRenderFunction renderer, VideoUserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, VideoUserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, VideoUserDataType pfcUserData)
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

bool SDLMediaPlayer::playVideoWithAudio(const std::string& filePath, SDL_WindowID winId, VideoRenderFunction renderer, VideoUserDataType rendererUserData, BeforePlaybackCallback beforePlaybackCallback, VideoUserDataType bpcUserData, PlaybackFinishedCallback playbackFinishedCallback, VideoUserDataType pfcUserData)
{
    setupPlayer(winId, renderer, rendererUserData, beforePlaybackCallback, bpcUserData, playbackFinishedCallback, pfcUserData);
    auto&& bpc = [this, filePath](const AVFormatContext* formatCtx, const AVCodecContext* videoCodecCtx, std::any userData) {
        SDLApp::runOnMainThread([&](void*) {
            // 在播放前创建纹理
            //auto windowScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(currentWindow));
            SizeI windowSize;
            if (SDL_GetWindowSizeInPixels(currentWindow, &windowSize.w, &windowSize.h))
            {
                // 创建SDL纹理
                currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
                    SDL_TEXTUREACCESS_STREAMING,
                    windowSize.width(), windowSize.height()));
                if (!currentTexture)
                {
                    logger.error("Failed to create SDL texture: {}", SDL_GetError());
                    SDL_Log("无法创建纹理: %s", SDL_GetError());
                    return;
                }
            }
            //auto props = SDL_CreateProperties();
            //SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER, );
            //texture.reset(SDL_CreateTextureWithProperties(renderer, props));
            }, 0, true/*等待执行结束*/);
        if (this->beforePlaybackCallback)
            this->beforePlaybackCallback(formatCtx, videoCodecCtx, this->bpcUserData);
        };
    auto&& rf = [this, bpc](const VideoFrameContext& frameCtx, VideoUserDataType userData) {
        if (!currentTexture)
            bpc(frameCtx.formatCtx, frameCtx.codecCtx, this->bpcUserData);
        // 在主线程渲染内容
        SDLApp::runOnMainThread([&](void*) {
            auto renderFrame = frameCtx.newFormatFrame;
            if (!renderFrame) // 转换格式失败
                return;
            if (!currentTexture) // 纹理未创建
            {
                logger.error("Failed to render video frame: SDL texture is null or not created.");
                SDL_Log("无法渲染视频帧，纹理为空或未创建。");
                return;
            }
            // 更新纹理
            SDL_UpdateYUVTexture(currentTexture.get(), nullptr,
                renderFrame->data[0], renderFrame->linesize[0],
                renderFrame->data[1], renderFrame->linesize[1],
                renderFrame->data[2], renderFrame->linesize[2]);
            double windowWidthHeightRatio = prevWindowSizeInPixel.w / (double)prevWindowSizeInPixel.h;
            double frameWidthHeightRatio = renderFrame->width / (double)renderFrame->height;

            SDL_FRect frameRectF{ 0.0f, 0.0f, (float)renderFrame->width, (float)renderFrame->height };

            SDL_FRect targetRectF{ 0.0f, 0.0f, 0.0f, 0.0f };
            if (windowWidthHeightRatio > frameWidthHeightRatio)
            {
                targetRectF.w = renderFrame->height * frameWidthHeightRatio;
                targetRectF.h = renderFrame->height;
                targetRectF.x = (prevWindowSizeInPixel.w - renderFrame->width) / (double)2;
                targetRectF.y = 0;
            }
            else
            {
                targetRectF.w = renderFrame->width;
                targetRectF.h = renderFrame->width / frameWidthHeightRatio;
                targetRectF.x = 0;
                targetRectF.y = (prevWindowSizeInPixel.h - renderFrame->height) / (double)2;
            }
            // 渲染
            SDL_RenderClear(currentRenderer);
            SDL_RenderTexture(currentRenderer, currentTexture.get(), &frameRectF, &targetRectF);
            //SDL_RenderTexture(currentRenderer, currentTexture.get(), nullptr, nullptr);
            SDL_RenderPresent(currentRenderer);
            }, 0, true/*等待执行结束*/);
        if (this->renderFunction)
            this->renderFunction(frameCtx, this->rendererUserData);
        };
    auto&& pfc = [&](std::any userData) {
        SDLApp::runOnMainThread([&](void*) {
            // 清空窗口
            SDL_SetRenderDrawColor(currentRenderer, 0, 0, 0, 255); // 全黑
            SDL_RenderClear(currentRenderer); // 绘制清除颜色：黑色
            SDL_RenderPresent(currentRenderer); // 双缓冲，需要将这一帧覆盖当前显示帧
            }); // 默认参数：等待执行结束
        if (this->playbackFinishedCallback)
            this->playbackFinishedCallback(this->pfcUserData);
        };
    auto&& frameSwitchOptionsCallback = [this, bpc](VideoFrameSwitchOptions& to, const VideoFrameContext& frameCtx, VideoUserDataType userData) {
        if (!frameCtx.frameSwitchOptions.enabled)
        {
            bpc(frameCtx.formatCtx, frameCtx.codecCtx, this->bpcUserData);
            prevWindowSizeInPixel = SizeI{}; // 如果未启用转换选项，重置之前的窗口大小缓存
        }
        to.enabled = true;
        to.format = AV_PIX_FMT_YUV420P;
        to.size = frameCtx.frameSwitchOptions.size;
        SizeI frameSize;
        if (frameCtx.swRawFrame) // 使用软件解码帧中的宽高
            frameSize.setSize(frameCtx.swRawFrame->width, frameCtx.swRawFrame->height);
        else if (frameCtx.codecCtx) // 使用解码器上下文中的宽高
            frameSize.setSize(frameCtx.codecCtx->width, frameCtx.codecCtx->height);
        SDLApp::runOnMainThread([&](void*) {
            SizeI windowSize;
            if (!SDL_GetWindowSizeInPixels(currentWindow, &windowSize.w, &windowSize.h))
                return;
            if (windowSize == prevWindowSizeInPixel) // 窗口大小未变化则不处理
                return;
            // 如果成功，保存当前窗口大小用于缩放视频帧
            to.size = windowSize;
            prevWindowSizeInPixel = windowSize; // 更新之前的窗口大小
            // 计算宽高比
            double textureWidthHeightRatio = windowSize.width() / (double)windowSize.height();
            double frameWidthHeightRatio = frameSize.width() / (double)frameSize.height();
            if (textureWidthHeightRatio > frameWidthHeightRatio)
            {
                to.size.w = static_cast<int>(windowSize.height() * frameWidthHeightRatio);
                to.size.h = windowSize.height();
            }
            else
            {
                to.size.w = windowSize.width();
                if (frameSize.width())
                    to.size.h = static_cast<int>(windowSize.width() / frameWidthHeightRatio);
            }
            // 创建SDL纹理
            currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                to.size.width(), to.size.height()));
            }, 0, true/*等待执行结束*/);
        };
    ;
    MediaPlayer::VideoPlayOptions videoOptions;
    videoOptions.enableHardwareDecoding = false;
    videoOptions.clockSyncFunction = videoClockSyncFunction;
    videoOptions.renderer = rf;
    //videoOptions.rendererUserData = nullptr;
    videoOptions.streamIndexSelector = streamIndexSelector;
    videoOptions.frameSwitchCallback = frameSwitchOptionsCallback;
    //videoOptions.frameSwitchCallbackUserData = nullptr;
    MediaPlayer::AudioPlayOptions audioOptions;
    audioOptions.streamIndexSelector = streamIndexSelector;
    audioOptions.clockSyncFunction = audioClockSyncFunction;
    return MediaPlayer::play(filePath, videoOptions, audioOptions);
}

