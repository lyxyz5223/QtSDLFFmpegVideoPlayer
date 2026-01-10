#include "SDLMediaPlayer.h"
#include "SDLApp.h"

void SDLMediaPlayer::setupPlayer()
{
    SDLApp::runOnMainThread([&](void*) {
        this->currentWindow = SDL_GetWindowFromID(currentWindowId);
        this->currentRenderer = SDL_GetRenderer(currentWindow);

        // 在播放前不需要创建纹理，frameSwitchOptionsCallback时会创建
        }, 0, true/*等待执行结束*/);
}

void SDLMediaPlayer::renderVideoFrame(const VideoFrameContext& frameCtx, VideoUserDataType userData)
{
    if (!currentTexture) // 纹理未创建
    {
        logger.error("Failed to render video frame: SDL texture is null or not created.");
        SDL_Log("无法渲染视频帧，纹理为空或未创建。");
        return;
    }
    // 在主线程渲染内容
    SDLApp::runOnMainThread([&](void*) {
        auto renderFrame = frameCtx.newFormatFrame;
        if (!renderFrame) // 转换格式失败
            return;
        // 更新纹理
        SDL_UpdateYUVTexture(currentTexture.get(), nullptr,
            renderFrame->data[0], renderFrame->linesize[0],
            renderFrame->data[1], renderFrame->linesize[1],
            renderFrame->data[2], renderFrame->linesize[2]);
        SizeI frameSize{ renderFrame->width, renderFrame->height };
        SDL_FRect targetRectF = calculateTextureRectF(frameSize, prevWindowSizeInPixel, texturePosition);
        SDL_FRect frameRectF{ 0.0f, 0.0f, (float)renderFrame->width, (float)renderFrame->height };
        // 渲染
        SDL_RenderClear(currentRenderer);
        SDL_RenderTexture(currentRenderer, currentTexture.get(), &frameRectF, &targetRectF);
        //SDL_RenderTexture(currentRenderer, currentTexture.get(), nullptr, nullptr);
        SDL_RenderPresent(currentRenderer);
        }, 0, true/*等待执行结束*/);
}

void SDLMediaPlayer::cleanupPlayer()
{
    // 播放完毕，清空窗口内容
    SDLApp::runOnMainThread([&](void*) {
        if (!currentRenderer)
            return;
        // 清空窗口
        SDL_SetRenderDrawColor(currentRenderer, 0, 0, 0, 255); // 全黑
        SDL_RenderClear(currentRenderer); // 绘制清除颜色：黑色
        SDL_RenderPresent(currentRenderer); // 双缓冲，需要将这一帧覆盖当前显示帧
        }); // 默认参数：等待执行结束
}

void SDLMediaPlayer::frameSwitchOptionsCallback(VideoFrameSwitchOptions& to, const VideoFrameContext& frameCtx, VideoUserDataType userData)
{
    // 如果上一帧未启用转换选项，则说明要么是第一次转换，要么是关闭了转换选项（在这里只能是第一次转换，因为下文确定开启转换）
    if (!frameCtx.frameSwitchOptions.enabled)
        prevWindowSizeInPixel = SizeI{}; // 第一次转换，重置之前的窗口大小缓存
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
        calculateTextureSize(to.size, frameSize, windowSize, scalingMode);
        // 创建SDL纹理
        currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            to.size.width(), to.size.height()));
        if (!currentTexture)
        {
            logger.error("Failed to create SDL texture: {}", SDL_GetError());
            SDL_Log("无法创建纹理: %s", SDL_GetError());
        }
        }, 0, true/*等待执行结束*/);
}

template <typename SizeBaseType>
SDL_FRect SDLMediaPlayer::calculateTextureRectF(const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, TexturePosition position)
{
    double containerWidthHeightRatio = containerSize.w / (double)containerSize.h;
    double frameWidthHeightRatio = frameSize.w / (double)frameSize.h;

    SDL_FRect targetRectF{ 0.0f, 0.0f, 0.0f, 0.0f };
    // 自适应缩放
    if (containerWidthHeightRatio > frameWidthHeightRatio)
    {
        targetRectF.w = frameSize.height() * frameWidthHeightRatio;
        targetRectF.h = frameSize.height();
        targetRectF.x = (containerSize.width() - frameSize.width()) / (double)2;
        targetRectF.y = 0;
    }
    else
    {
        targetRectF.w = frameSize.width();
        targetRectF.h = frameSize.width() / frameWidthHeightRatio;
        targetRectF.x = 0;
        targetRectF.y = (containerSize.height() - frameSize.height()) / (double)2;
    }
    return targetRectF;
}

template <typename SizeBaseType>
SDLMediaPlayer::Size<SizeBaseType>& SDLMediaPlayer::calculateTextureSize(Size<SizeBaseType>& target, const Size<SizeBaseType>& frameSize, const Size<SizeBaseType>& containerSize, ScalingMode mode)
{
    // 计算宽高比
    double containerWidthHeightRatio = containerSize.width() / (double)containerSize.height();
    double frameWidthHeightRatio = frameSize.width() / (double)frameSize.height();
    switch (mode)
    {
    case SDLMediaPlayer::ScalingMode::ScaleToFit:
    {
        if (containerWidthHeightRatio > frameWidthHeightRatio)
        {
            target.setWidth(static_cast<int>(containerSize.height() * frameWidthHeightRatio));
            target.setHeight(containerSize.height());
        }
        else
        {
            target.setWidth(containerSize.width());
            if (frameSize.width())
                target.setHeight(static_cast<int>(containerSize.width() / frameWidthHeightRatio));
        }
        break;
    }
    case SDLMediaPlayer::ScalingMode::ScaleToFill:
        break;
    case SDLMediaPlayer::ScalingMode::StretchToFill:
        break;
    default:
        break;
    }
    return target;
}

bool SDLMediaPlayer::play(const std::string& filePath, SDL_WindowID winId, bool enableHardwareDecoding)
{
    this->currentWindowId = winId;
    this->currentTexture.reset();
    setupPlayer();
    MediaPlayer::MediaPlayOptions mediaOptions;
    mediaOptions.renderer = std::bind(&SDLMediaPlayer::renderVideoFrame, this, std::placeholders::_1, std::placeholders::_2);
    mediaOptions.rendererUserData = std::any{};
    mediaOptions.frameSwitchOptionsCallback = std::bind(&SDLMediaPlayer::frameSwitchOptionsCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    mediaOptions.frameSwitchOptionsCallbackUserData = std::any{};
    mediaOptions.enableHardwareDecoding = enableHardwareDecoding;
    bool r = MediaPlayer::play(filePath, mediaOptions);
    return r;
}

void SDLMediaPlayer::startEvent(MediaPlaybackStateChangeEvent* e)
{
}

void SDLMediaPlayer::stopEvent(MediaPlaybackStateChangeEvent* e)
{
    cleanupPlayer();
}

