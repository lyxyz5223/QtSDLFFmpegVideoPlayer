#include <FrameProcessor.h>


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

void SDLMediaPlayer::renderVideoFrame(const VideoDecodedFrameContext& frameCtx, VideoUserDataType userData)
{
    VideoRenderUserData* ud = std::any_cast<VideoRenderUserData*>(userData);
    if (!ud->processor)
        ud->processor = std::make_shared<VideoFrameProcessor>(logger, brightness, contrast, saturation, hue);
    if (!ud->processor) return;
    
    SizeI windowSize;
    if (SDL_GetWindowSizeInPixels(currentWindow, &windowSize.w, &windowSize.h))
    {
        if (windowSize != prevWindowSizeInPixel || !currentTexture) // 窗口大小变化或者没有创建纹理则创建
        {
            prevWindowSizeInPixel = windowSize;
            SizeI targetSize;
            calculateTextureSize(targetSize, SizeI{ frameCtx.filteredFrame->width, frameCtx.filteredFrame->height }, windowSize, scalingMode);
            currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_TARGET, targetSize.width(), targetSize.height()));
            ud->processor->setProcessedFrameSize(targetSize);
        }
    }
    if (!currentTexture) // 纹理创建失败
        logger.error("Failed to render video frame: SDL texture is not created or failed to be created.");

    auto fltdFrame = ud->processor->process(frameCtx);
    if (!fltdFrame) return;
    // 在主线程渲染内容
    SDLApp::runOnMainThread([&renderFrame=fltdFrame, this](void*) {
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

//void SDLMediaPlayer::frameSwitchOptionsCallback(VideoFrameSwitchOptions& to, const VideoDecodedFrameContext& frameCtx, VideoUserDataType userData)
//{
//    // 如果上一帧未启用转换选项，则说明要么是第一次转换，要么是关闭了转换选项（在这里只能是第一次转换，因为下文确定开启转换）
//    if (!frameCtx.frameSwitchOptions.enabled)
//        prevWindowSizeInPixel = SizeI{}; // 第一次转换，重置之前的窗口大小缓存
//    to.enabled = true;
//    to.format = AV_PIX_FMT_YUV420P;
//    to.size = frameCtx.frameSwitchOptions.size;
//    SizeI frameSize;
//    if (frameCtx.swRawFrame) // 使用软件解码帧中的宽高
//        frameSize.setSize(frameCtx.swRawFrame->width, frameCtx.swRawFrame->height);
//    else if (frameCtx.codecCtx) // 使用解码器上下文中的宽高
//        frameSize.setSize(frameCtx.codecCtx->width, frameCtx.codecCtx->height);
//    SDLApp::runOnMainThread([&](void*) {
//        SizeI windowSize;
//        if (!SDL_GetWindowSizeInPixels(currentWindow, &windowSize.w, &windowSize.h))
//            return;
//        if (windowSize == prevWindowSizeInPixel) // 窗口大小未变化则不处理
//            return;
//        // 如果成功，保存当前窗口大小用于缩放视频帧
//        to.size = windowSize;
//        prevWindowSizeInPixel = windowSize; // 更新之前的窗口大小
//        calculateTextureSize(to.size, frameSize, windowSize, scalingMode);
//        // 创建SDL纹理
//        currentTexture.reset(SDL_CreateTexture(currentRenderer, SDL_PIXELFORMAT_YV12,
//            SDL_TEXTUREACCESS_STREAMING,
//            to.size.width(), to.size.height()));
//        if (!currentTexture)
//        {
//            logger.error("Failed to create SDL texture: {}", SDL_GetError());
//            SDL_Log("无法创建纹理: %s", SDL_GetError());
//        }
//        }, 0, true/*等待执行结束*/);
//}

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
    return play(filePath, winId, enableHardwareDecoding ? VideoDecodeType::Hardware : VideoDecodeType::Software);
}

bool SDLMediaPlayer::play(const std::string& filePath, SDL_WindowID winId, VideoDecodeType videoDecodeType)
{
    this->currentWindowId = winId;
    this->currentTexture.reset();
    setupPlayer();
    MediaPlayer::MediaPlayOptions mediaOptions;
    mediaOptions.decodeType = videoDecodeType;
    mediaOptions.renderer = std::bind(&SDLMediaPlayer::renderVideoFrame, this, std::placeholders::_1, std::placeholders::_2);
    VideoRenderUserData renderUserData;
    mediaOptions.rendererUserData = &renderUserData;

    UniquePtrD<FFmpegFrameFilterGraph> videoFilterGraph{ nullptr };
    SharedPtr<FFmpegHwFrameVideoScaleCudaFilter> scaleCudaFilter{ nullptr };
    mediaOptions.videoFrameFilterGraphCreator = [
        &videoFilterGraph, &scaleCudaFilter, this
    ](std::vector<IFrameFilterGraph*>& outFilterGraphs, const VideoDecodedFrameContext& frameContext, VideoUserDataType userData) -> bool {
        if (!videoFilterGraph)
        {
            auto& streamIndex = frameContext.streamIndex;
            auto& formatCtx = frameContext.formatCtx;
            auto& codecCtx = frameContext.codecCtx;
            videoFilterGraph = std::make_unique<FFmpegFrameFilterGraph>(StreamType::STVideo, formatCtx, codecCtx, streamIndex);
            //scaleCudaFilter = std::make_shared<FFmpegHwFrameVideoScaleCudaFilter>(StreamType::STVideo, formatCtx, codecCtx, streamIndex, -1, -1, AV_PIX_FMT_NONE, "", true, FFmpegHwFrameVideoScaleCudaFilter::Disable);
            //videoFilterGraph->addFilter(scaleCudaFilter);
            videoFilterGraph->configureFilterGraph();
        }
        outFilterGraphs.push_back(videoFilterGraph.get());
        return true;
    };
    mediaOptions.videoFrameFilterGraphCreatorUserData = VideoUserDataType{};

    auto audioFilterGraph = UniquePtrD<FFmpegFrameFilterGraph>(nullptr);
    auto equalizerFilter = SharedPtr<FFmpegFrameAudio10BandEqualizerFilter>(nullptr);
    auto volumeFilter = SharedPtr<FFmpegFrameVolumeFilter>(nullptr);
    auto speedFilter = SharedPtr<FFmpegFrameAudioSpeedFilter>(nullptr);
    auto lastEqualizerEnabledState = false;
    mediaOptions.audioFrameFilterGraphCreator = [
        this, &audioFilterGraph, &equalizerFilter, &volumeFilter, &speedFilter, &lastEqualizerEnabledState
    ](std::vector<IFrameFilterGraph*>& outFilterGraphs, bool& shouldResetSwrContext, const AudioDecodedFrameContext& frameContext, AudioUserDataType userData) -> bool {
        if (!audioFilterGraph)
        {
            auto& streamIndex = frameContext.streamIndex;
            auto& formatCtx = frameContext.formatCtx;
            auto& codecCtx = frameContext.codecCtx;
            audioFilterGraph = std::make_unique<FFmpegFrameFilterGraph>(StreamType::STAudio, formatCtx, codecCtx, streamIndex);
            equalizerFilter = std::make_shared<FFmpegFrameAudio10BandEqualizerFilter>(StreamType::STAudio, formatCtx, codecCtx, streamIndex);
            volumeFilter = std::make_shared<FFmpegFrameVolumeFilter>(StreamType::STAudio, formatCtx, codecCtx, streamIndex, volume);
            speedFilter = std::make_shared<FFmpegFrameAudioSpeedFilter>(StreamType::STAudio, formatCtx, codecCtx, streamIndex, speed);
            if (lastEqualizerEnabledState)
                audioFilterGraph->addFilter(equalizerFilter);
            audioFilterGraph->addFilter(volumeFilter);
            if (speed != 1.0)
                audioFilterGraph->addFilter(speedFilter); // 默认1倍速，不需要该滤镜，否则需要添加滤镜
            audioFilterGraph->configureFilterGraph();
        }
        outFilterGraphs.push_back(audioFilterGraph.get());

        double oldSpeed = speedFilter->speed();
        speedFilter->setSpeed(speed);
        volumeFilter->setVolume(volume);
        equalizerFilter->setBandGains(equalizerBandGains);
        Queue<std::function<void()>> postFilterGraphConfigTasks;
        if (oldSpeed != speed && oldSpeed == 1.0)
        {
            // 从1x倍速改为变速
            // 重建滤镜图
            postFilterGraphConfigTasks.push([&] {
                audioFilterGraph->addFilter(speedFilter);
                });
        }
        else if (oldSpeed != speed && speed == 1.0)
        {
            // 1x取消倍速，防止音质受损
            // 重建滤镜图
            postFilterGraphConfigTasks.push([&] {
                audioFilterGraph->removeFilter(speedFilter.get());
                });
        }
        else if (oldSpeed > 2.0 && speed < 2.0)
        {
            // 重建滤镜图
            postFilterGraphConfigTasks.push([&] {}); // 入队一个空的任务，后续判断postFilterGraphConfigTasks是否为空来决定是否重建滤镜图
        }
        if (lastEqualizerEnabledState != isEqualizerEnabled)
        {
            lastEqualizerEnabledState = isEqualizerEnabled;
            // 重建滤镜图
            postFilterGraphConfigTasks.push([&] {
                if (isEqualizerEnabled)
                    audioFilterGraph->addFilter(equalizerFilter);
                else
                    audioFilterGraph->removeFilter(equalizerFilter.get());
                });
        }
        if (!postFilterGraphConfigTasks.empty())
        {
            audioFilterGraph->resetFilterGraph();
            while (!postFilterGraphConfigTasks.empty())
            {
                auto& task = postFilterGraphConfigTasks.front();
                task();
                postFilterGraphConfigTasks.pop();
            }
            audioFilterGraph->configureFilterGraph();
        }
        return true;
    };
    mediaOptions.audioFrameFilterGraphCreatorUserData = AudioUserDataType{};

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

