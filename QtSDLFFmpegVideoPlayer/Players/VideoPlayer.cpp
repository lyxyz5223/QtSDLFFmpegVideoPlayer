#include "VideoPlayer.h"
#include <FrameProcessor.h>

bool VideoPlayer::prepareBeforePlayback()
{
    waitStopped.set(false);
    auto demuxerMode = this->demuxerMode;
    auto requestTaskQueueHandlerMode = this->requestTaskQueueHandlerMode;
    // 初始化解复用器
    try {
        auto& psv = playbackStateVariables;
        if (demuxerMode == ComponentWorkMode::Internal)
        {
            psv.demuxer.store(internalDemuxer.get());
            psv.demuxer.load()->setPacketEnqueueCallback(playbackStateVariables.demuxerStreamType, std::bind(&VideoPlayer::packetEnqueueCallback, this));
            psv.demuxer.load()->openAndSelectStreams(psv.filePath, psv.demuxerStreamType, psv.playOptions.streamIndexSelector);
        }
        else/* if (demuxerMode == DemuxerMode::External)*/
            psv.demuxer.store(externalDemuxer.get());
        if (!psv.demuxer.load()) // 解复用器不存在，通常为外部解复用器指针为空
            throw std::runtime_error("Demuxer is nullptr.");
        psv.demuxer.load()->setMaxPacketQueueSize(psv.demuxerStreamType, MAX_VIDEO_PACKET_QUEUE_SIZE);
        psv.demuxer.load()->setMinPacketQueueSize(psv.demuxerStreamType, MIN_VIDEO_PACKET_QUEUE_SIZE);
        psv.formatCtx = psv.demuxer.load()->getFormatContext();
        psv.streamIndex = psv.demuxer.load()->getStreamIndex(psv.demuxerStreamType);
        psv.packetQueue = psv.demuxer.load()->getPacketQueue(psv.demuxerStreamType);

        if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        {
            psv.requestQueueHandler = internalRequestTaskQueueHandler.get();
            addThreadStateHandlersContext(internalRequestTaskQueueHandler.get());
        }
        else
            psv.requestQueueHandler = externalRequestTaskQueueHandler;
        if (!psv.requestQueueHandler) // 请求任务队列处理器不存在，通常为外部请求任务队列处理器指针为空
            throw std::runtime_error("RequestTaskQueueHandler is nullptr.");
    }
    catch (std::runtime_error e) {
        logger.error("Failed to open and select streams: {}", e.what());
        resetPlayer();
        return false;
    }
    if (playbackStateVariables.streamIndex < 0)
    {
        logger.warning("No video stream selected.");
        resetPlayer();
        return false;
    }
    //// 打开文件
    //if (!openInput())
    //{
    //    resetPlayer();
    //    return false;
    //}
    //// 查找流信息
    //if (!findStreamInfo())
    //{
    //    resetPlayer();
    //    return false;
    //}
    //// 查找并选择视频流
    //if (!findAndSelectVideoStream())
    //{
    //    resetPlayer();
    //    return false;
    //}

    // 寻找并打开解码器
    if (!findAndOpenVideoDecoder())
    {
        resetPlayer();
        return false;
    }
    setPlayerState(PlayerState::Playing);
    return true;
}

bool VideoPlayer::playVideoFile()
{
    // 启动处理线程
    if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        playbackStateVariables.requestQueueHandler->start();
    // 启动读包
    //std::thread threadReadPackets(&VideoPlayer::readPackets, this);
    if (demuxerMode == ComponentWorkMode::Internal)
        playbackStateVariables.demuxer.load()->start(); // 启动解复用器读取线程
    // 启动包转视频流
    std::thread threadPacket2VideoFrames(&VideoPlayer::packet2VideoFrames, this);
    // 启动渲染视频
    std::thread threadRenderVideo(&VideoPlayer::renderVideo, this);
    // 等待线程结束
    if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        playbackStateVariables.requestQueueHandler->waitStop();
    //if (threadReadPackets.joinable())
    //    threadReadPackets.join();
    if (threadPacket2VideoFrames.joinable())
        threadPacket2VideoFrames.join();
    if (threadRenderVideo.joinable())
        threadRenderVideo.join();
    if (demuxerMode == ComponentWorkMode::Internal)
        playbackStateVariables.demuxer.load()->waitStop(); // 等待解复用器停止
    return true;
}

void VideoPlayer::cleanupAfterPlayback()
{
    // 恢复播放器状态
    resetPlayer();
    // 通知停止
    waitStopped.setAndNotifyAll(true);
}


//bool VideoPlayer::findAndSelectVideoStream()
//{
//    std::vector<StreamIndexType> streamIndexesList;
//    // 查找视频流和音频流
//    for (uint64_t i = 0; i < playbackStateVariables.formatCtx->nb_streams; ++i)
//        if (playbackStateVariables.formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
//            streamIndexesList.emplace_back(i);
//    StreamIndexType si = -1;
//    auto& streamIndexSelector = playbackStateVariables.playOptions.streamIndexSelector;
//    if (!streamIndexSelector)
//        return false;
//    bool rst = streamIndexSelector(si, StreamType::STVideo, streamIndexesList, playbackStateVariables.formatCtx.get(), playbackStateVariables.codecCtx.get());
//    if (rst)
//    {
//        if (si >= 0 && si < static_cast<StreamIndexType>(playbackStateVariables.formatCtx->nb_streams))
//            playbackStateVariables.streamIndex = si;
//        else
//            playbackStateVariables.streamIndex = -1;
//    }
//    return rst;
//}

bool VideoPlayer::findAndOpenVideoDecoder()
{
    bool useHardwareDecoding = getIsHardwareDecodingEnabled();
    auto rst = MediaDecodeUtils::findAndOpenVideoDecoder(&logger,
        playbackStateVariables.formatCtx, playbackStateVariables.streamIndex, playbackStateVariables.codecCtx,
        useHardwareDecoding, MAX_VIDEO_HARDWARE_EXTRA_FRAME_SIZE, &playbackStateVariables.hwDeviceType, &playbackStateVariables.hwPixelFormat);
    if (!useHardwareDecoding)
    {
        playbackStateVariables.hwDeviceType = AV_HWDEVICE_TYPE_NONE;
        playbackStateVariables.hwPixelFormat = AV_PIX_FMT_NONE;
    }
    return rst;
}

//void VideoPlayer::readPackets()
//{
//    auto& threadStateManager = playbackStateVariables.threadStateManager;
//    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::ReadingThread);
//    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
//    // 视频解码和显示循环
//    while (true)
//    {
//        if (waitObj.isBlocking())
//            waitObj.block();
//
//        if (shouldStop())
//            break;
//        auto oldPktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
//        if (oldPktQueueSize >= MAX_VIDEO_PACKET_QUEUE_SIZE)
//        {
//            waitObj.pause();
//            continue;
//        }
//        // Read frame from the format context 读取帧
//        AVPacket* pkt = nullptr;
//        if (!MediaDecodeUtils::readFrame(&logger, playbackStateVariables.formatCtx.get(), pkt, true))
//        {
//            if (pkt)
//                av_packet_free(&pkt); // 释放包
//            logger.trace("Read frame finished.");
//            //break; // 读取结束，退出循环
//            // 读取结束，暂停线程，等待通知
//            waitObj.pause();
//            continue;
//        }
//        if (pkt->stream_index != playbackStateVariables.streamIndex)
//        {
//            av_packet_free(&pkt); // 释放不需要的包
//            continue;
//        }
//        enqueue(playbackStateVariables.packetQueue, pkt);
//        logger.trace("Pushed video packet, queue size: {}", oldPktQueueSize + 1);
//        if (oldPktQueueSize + 1 <= MIN_VIDEO_PACKET_QUEUE_SIZE)
//            threadStateManager.wakeUpById(ThreadIdentifier::DecodingThread);
//    }
//}

// 视频包解码线程函数
void VideoPlayer::packet2VideoFrames()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::Decoder);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();

        if (shouldStop()) // 收到停止信号，退出循环
            break;
        else if (playerState != PlayerState::Playing)
        {
            waitObj.pause();
            continue;
        }
        if (getQueueSize(playbackStateVariables.frameQueue) >= MAX_VIDEO_FRAME_QUEUE_SIZE)
        {
            waitObj.pause(); // 如果视频帧队列中有太多数据，等待消费掉一些再继续解码
            continue;
        }
        if (getQueueSize(*playbackStateVariables.packetQueue) < MIN_VIDEO_PACKET_QUEUE_SIZE)
            playbackStateVariables.demuxer.load()->wakeUp(); // 包队列数据过少，唤醒解复用器读取更多数据
        // 取出视频包
        AVPacket* videoPkt = nullptr;
        if (!tryDequeue(*playbackStateVariables.packetQueue, videoPkt))
        {
            waitObj.pause(); // 队列为空，先暂停线程
            continue; // 少了这句就会出现空包
        }
        if (!videoPkt) // 一定要过滤空包，否则avcodec_send_packet将会设置为EOF，之后将无法继续解包
            continue;
        logger.trace("Got video packet, current video packet queue size: {}", getQueueSize(*playbackStateVariables.packetQueue));
        UniquePtr<AVPacket> pktPtr{ videoPkt, constDeleterAVPacket };
        int aspRst = avcodec_send_packet(playbackStateVariables.codecCtx.get(), videoPkt);
        if (aspRst < 0 && aspRst != AVERROR(EAGAIN) && aspRst != AVERROR_EOF)
            continue;
        UniquePtr<AVFrame> frame{ av_frame_alloc(), constDeleterAVFrame }; // 用于存放解码后的视频帧
        while (avcodec_receive_frame(playbackStateVariables.codecCtx.get(), frame.get()) == 0)
        {
            AVFrame* refFrame{ av_frame_alloc() };
            if (av_frame_ref(refFrame, frame.get()) < 0)
            {
                av_frame_free(&refFrame);
                continue;
            }
            enqueue(playbackStateVariables.frameQueue, refFrame); // 放入待渲染队列
            // 通知渲染线程继续工作
            if (getQueueSize(playbackStateVariables.frameQueue) <= MIN_VIDEO_FRAME_QUEUE_SIZE)
                threadStateManager.wakeUpById(ThreadIdentifier::Renderer);
        }
    }
}


// 视频渲染
void VideoPlayer::renderVideo()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::Renderer);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };


    //auto& codecCtx = videoCodecCtx;
    double timeBase = av_q2d(playbackStateVariables.formatCtx->streams[playbackStateVariables.streamIndex]->time_base);
    double frameDuration = timeBase; // 备用方案：使用时间基计算
    // 计算帧持续时间
    AVRational frameRate = playbackStateVariables.formatCtx->streams[playbackStateVariables.streamIndex]->avg_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0)
        frameDuration = av_q2d(av_inv_q(frameRate)); // 每帧的秒数，用于备选：计算视频时钟
    // 软硬件
    SharedPtr<AVFrame> rawFrame{ makeSharedFrame(nullptr) };

    //UniquePtr<AVFrame> rawFrame{ makeUniqueFrame(nullptr) };
    //UniquePtr<AVFrame> switchedFrame{ makeUniqueFrame() }; // 用于存放转换为新格式的视频帧
    //UniquePtr<uint8_t> bufferSwitchedFrame = { nullptr, [](uint8_t* p) { if (p) av_free(p); } };
    //UniquePtr<SwsContext> swsCtx{ nullptr, [](SwsContext* ctx) { if (ctx) sws_freeContext(ctx); } };
    AVPixelFormat hwFramePixFmt = AV_PIX_FMT_NONE; // 硬件解码后数据传输的目标格式（即硬件格式），AV_PIX_FMT_NONE表示自动选择，后面将自动识别
    auto swFramePixFmt = playbackStateVariables.codecCtx->pix_fmt; // 软件解码的像素格式，默认为解码器输出的格式


    // IIR 滤波，用于平滑过渡视频与音频的时间差
    double avgDiff = 0.0;
    // 内部视频时钟
    double videoClockInside = 0.0; // 用于videoClock时钟的计算
    int64_t realtimeClockStart = 0;

    SharedPtr<FrameProcessor> frameProcessor = std::make_shared<FrameProcessor>(logger);

    // 帧格式转换选项
    //auto& switchFormat = playbackStateVariables.playOptions.frameSwitchOptions.format;
    //auto& newSize = playbackStateVariables.playOptions.frameSwitchOptions.size;
    //auto& enableFrameFormatSwitch = playbackStateVariables.playOptions.frameSwitchOptions.enabled;
    auto& renderer = playbackStateVariables.playOptions.renderer;
    auto& rendererUserData = playbackStateVariables.playOptions.rendererUserData;
    auto& videoClockSyncFunction = playbackStateVariables.playOptions.clockSyncFunction;
    
    auto& filterGraphStreamType = playbackStateVariables.filterGraphStreamType;
    auto& formatCtx = playbackStateVariables.formatCtx;
    auto& codecCtx = playbackStateVariables.codecCtx;
    auto& streamIndex = playbackStateVariables.streamIndex;
    
    auto& frameFilterGraphCreator = playbackStateVariables.playOptions.frameFilterGraphCreator;
    auto& frameFilterGraphCreatorUserData = playbackStateVariables.playOptions.frameFilterGraphCreatorUserData;

    // 帧上下文
    DecodedFrameContext frameCtx;
    frameCtx.formatCtx = formatCtx;
    frameCtx.codecCtx = codecCtx.get();
    frameCtx.streamIndex = streamIndex;
    frameCtx.hwDeviceType = playbackStateVariables.hwDeviceType;
    frameCtx.hwPixelFormat = playbackStateVariables.hwPixelFormat;

    //SharedPtr<IFrameFilter> noneFilter = std::make_shared<FFmpegFrameNoneFilter>(filterGraphStreamType, formatCtx, codecCtx.get(), streamIndex);
    //UniquePtr<FFmpegFrameFilterGraph> filterGraph = std::make_unique<FFmpegFrameFilterGraph>(filterGraphStreamType, formatCtx, codecCtx.get(), streamIndex);
    //filterGraph->configureFilterGraph();


    // 返回false说明要么失败要么需要更多帧
    auto getCustomFilterGraphsAndFilter = [&](AVFrame* srcFrame, SharedPtr<AVFrame>& filteredFrame) -> bool {
        if (frameFilterGraphCreator)
        {
            DecodedFrameContext frameCtx{ formatCtx, codecCtx.get(), streamIndex, srcFrame };
            std::vector<IFrameFilterGraph*> externalFilterGraphs;
            bool useFilterGraph = frameFilterGraphCreator(externalFilterGraphs, frameCtx, frameFilterGraphCreatorUserData);
            bool needMoreFrame = false;
            if (useFilterGraph)
            {
                for (auto& fg : externalFilterGraphs)
                {
                    if (!fg || !fg->isValid())
                        continue;
                    filteredFrame = frameProcessor->filterFrame(srcFrame, fg, needMoreFrame);
                    needMoreFrame = !filteredFrame; // 如果滤镜图没有输出，说明需要更多帧才能输出
                    if (needMoreFrame)
                        break; // 需要更多帧，停止当前帧的外部滤镜处理，继续解码下一帧
                }
            }
            if (needMoreFrame)
                return false;
        }
        return true;
        };

    // 视频渲染循环
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();
        auto timeBeforeGetFrame = std::chrono::high_resolution_clock::now();

        if (shouldStop()) // 收到停止信号，退出循环
            break;
        else if (playerState != PlayerState::Playing)
        {
            waitObj.pause();
            continue;
        }
        if (getQueueSize(playbackStateVariables.frameQueue) < MIN_VIDEO_FRAME_QUEUE_SIZE)
            threadStateManager.wakeUpById(ThreadIdentifier::Decoder);
        // 从队列中取出一个视频帧进行处理
        {
            AVFrame* frame = nullptr;
            if (!tryDequeue(playbackStateVariables.frameQueue, frame))
            {
                threadStateManager.wakeUpById(ThreadIdentifier::Decoder);
                waitObj.pause();
                continue; // 队列为空，继续下一轮循环
            }
            rawFrame.reset(frame, constDeleterAVFrame); // 取出队列头部元素
        }
        logger.trace("Got video frame, current video frame queue size: {}", getQueueSize(playbackStateVariables.frameQueue));
        auto timeBeforeTimeSync = std::chrono::high_resolution_clock::now();

        // 视频帧处理
        // 判断是否为硬件解码
        bool isHardwareDecoded = (rawFrame->format == playbackStateVariables.hwPixelFormat) && (playbackStateVariables.hwPixelFormat != AV_PIX_FMT_NONE);
        // 时钟记录开始时间，后续计算实时时钟使用
        if (!realtimeClockStart)
            realtimeClockStart = av_gettime();
        auto calcClock = [&] {
            playbackStateVariables.realtimeClock = (av_gettime() - realtimeClockStart) / 1000000.0;
            // 计算当前帧的时间戳
            if (rawFrame->pts != AV_NOPTS_VALUE)
            {
                videoClockInside = rawFrame->pts * timeBase;
                playbackStateVariables.videoClock.store(videoClockInside);
            }
            else
            {
                // 如果没有时间戳，使用累计时间，但是第一帧需要特殊处理
                //if (playbackStateVariables.isVideoClockStable.load()) // 时钟稳定，正常累加，否则不予理会
                    videoClockInside += frameDuration;
                playbackStateVariables.videoClock.store(videoClockInside);
            }
            };
        //auto rollbackClock = [&] {
        //    // 根据时钟同步需要，需要回退到上一帧的时间
        //    if (videoClockInside > frameDuration)
        //        playbackStateVariables.videoClock.store(videoClockInside - frameDuration);
        //    else
        //        playbackStateVariables.videoClock.store(0);
        //    };
        calcClock(); // 更新时钟
        // Seek后进行操作需要注意特殊处理，想到两个方案
        // 方案一：立即更新时钟+帧时间回退到上一帧，实现稳定时钟
        if (!playbackStateVariables.isVideoClockStable.load()) // 时钟不稳定
        {
            //calcClock(); // 立即更新时钟
            //rollbackClock(); // 根据时钟同步需要，需要回退到上一帧的时间
            //videoClockIncrementer.disable(); // 禁用自动时钟计时
            playbackStateVariables.isVideoClockStable.store(true); // 设置为稳定
        }
        // 方案二：暂时不更新时钟，下一帧再执行时钟同步，确保时钟最新
        //if (!playbackStateVariables.isVideoClockStable.load()) // 时钟不稳定
        //    playbackStateVariables.isVideoClockStable.store(true); // 设置为稳定
        //else { /*时钟同步*/ }
        // 音视频同步
        if (videoClockSyncFunction)
        {
            int64_t sleepTime = 0;
            bool frameShouldDrop = false;
            bool rst = videoClockSyncFunction(playbackStateVariables.videoClock, playbackStateVariables.isVideoClockStable, playbackStateVariables.realtimeClock, playbackStateVariables.formatCtx, playbackStateVariables.codecCtx.get(), playbackStateVariables.streamIndex, sleepTime, frameShouldDrop);
            if (rst && sleepTime != 0)
            {
                if (frameShouldDrop)
                {
                    logger.trace("Video drop frame to catch up: {} ms", -sleepTime);
                    continue; // 丢弃当前帧，继续下一帧解码
                }
                //if (sleepTime > 2 * frameDuration * 1000)
                //{
                //    // 需要等待
                //    logger.info("Video sleep: {} ms", sleepTime);
                //    ThreadSleepMs(sleepTime); // 后续可以替换为可打断的睡眠，防止睡眠时间过长导致收不到阻塞/退出信号
                //}
                //else if (sleepTime > frameDuration * 1000)
                //{
                //    // 需要等待
                //    logger.info("Video sleep: {} ms", sleepTime);
                //    ThreadSleepMs(frameDuration * 1000); // 后续可以替换为可打断的睡眠，防止睡眠时间过长导致收不到阻塞/退出信号
                //}
                //else 
                if (sleepTime > 0)
                {
                    // 需要等待
                    logger.trace("Video sleep: {} ms", sleepTime);
                    ThreadSleepMs(sleepTime); // 后续可以替换为可打断的睡眠，防止睡眠时间过长导致收不到阻塞/退出信号
                }
                else if (sleepTime < -300) // 超过x ms就跳帧
                {
                    // 落后太多，跳过帧
                    logger.trace("Video drop frame to catch up: {} ms", -sleepTime);
                    continue;
                }
                else if (sleepTime < 0)
                {
                    // 稍微落后，加速（不睡眠）播放，无伤大雅
                    logger.trace("Video is slightly behind: {} ms", -sleepTime);
                }
            }
        }
        auto timeBeforeFilter = std::chrono::high_resolution_clock::now();


        // 获取到软件帧（硬件帧->软件帧，或软件帧本身）后，使用滤波器处理得到最终帧
        SharedPtr<AVFrame> filteredFrame{ nullptr };
        bool shouldResetSwsContext = false; // 是否需要重置sws上下文，通常为选项发生变化时需要重置
        // 允许外部添加滤镜图进行处理
        if (!getCustomFilterGraphsAndFilter(rawFrame.get(), filteredFrame))
            continue;

        if (isHardwareDecoded && hwFramePixFmt == AV_PIX_FMT_NONE)
            hwFramePixFmt = getHwFramePixelFormat(rawFrame->hw_frames_ctx);
            //hwFramePixFmt = codecCtx->sw_pix_fmt;

        // 帧解析完成，更新帧上下文
        frameCtx.rawFrame = rawFrame.get();
        frameCtx.filteredFrame = filteredFrame.get();
        frameCtx.isHardwareDecoded = isHardwareDecoded;
        frameCtx.hwFramePixelFormat = hwFramePixFmt;

        // 渲染视频帧
        auto timeBeforeRender = std::chrono::high_resolution_clock::now();
        if (renderer) renderer(frameCtx, rendererUserData);
        VideoRenderEvent videoRenderEvent{ &frameCtx };
        event(&videoRenderEvent);
        auto timeAfterRender = std::chrono::high_resolution_clock::now();
        
        // 统计处理时间，用于分析性能瓶颈
        auto getFrameTimeInterval = timeBeforeTimeSync - timeBeforeGetFrame;
        auto timeSyncTimeInterval = timeBeforeFilter - timeBeforeTimeSync;
        auto switchTimeInterval = timeBeforeRender - timeBeforeFilter;
        auto renderTimeInterval = timeAfterRender - timeBeforeRender;
        auto frameRenderFullTime = getFrameTimeInterval + timeSyncTimeInterval + switchTimeInterval + renderTimeInterval;
        logger.trace("Current frame: full time: {}, get frame time: {}, time sync time: {}, filter time: {}, render time: {}", frameRenderFullTime.count(), getFrameTimeInterval.count(), timeSyncTimeInterval.count(), switchTimeInterval.count(), renderTimeInterval.count());
    }
}

/*
    // 在析构的时候自动计算视频当前播放时间
    class VideoClockAutoIncrementObj {
        std::function<void()> calcClock;
        AtomicDouble& videoClock;
        double& videoClockInside;
        double& realtimeClock;
        int64_t& realtimeClockStart;
        int64_t& pts;
        double& frameDuration;
        double& timeBase;
        bool enabled{ true }; // 默认启用
    public:
        VideoClockAutoIncrementObj(std::function<void()> calcClock, AtomicDouble& videoClock, double& videoClockInside, double& realtimeClock, int64_t& realtimeClockStart, int64_t& pts, double& frameDuration, double& timeBase)
            : calcClock(calcClock), videoClock(videoClock), videoClockInside(videoClockInside), realtimeClock(realtimeClock), realtimeClockStart(realtimeClockStart), pts(pts), frameDuration(frameDuration), timeBase(timeBase) {
            if (realtimeClockStart == 0)
            {
                realtimeClockStart = av_gettime();
            }
        }
        ~VideoClockAutoIncrementObj() {
            //SDL_Log("FrameDuration: %f, VideoClock: %f, AudioClock: %f", frameDuration, videoClock.load(), audioClock.load() / 1000000.0f);
            if (enabled)
                calcClock();
        }
        void enable() { enabled = true; } // 启用功能
        void disable() { enabled = false; } // 禁用功能
    } videoClockIncrementer(calcClock, playbackStateVariables.videoClock, videoClockInside, playbackStateVariables.realtimeClock, realtimeClockStart, videoFrame->pts, frameDuration, timeBase);
*/

/*
    // 先进行视频帧提前/落后计算
    if (!audioPlaybackFinished) // 如果音频还在播放
    {
        // 精确的帧率控制
        double sleepTime = videoClock - audioClock.load();
        // 误差低通滤波
        avgDiff = avgDiff * 0.9 + sleepTime * 0.1;   // 简单 IIR 滤波

        if (avgDiff > 0.01)
        { // 10 ms 以上才处理
            uint64_t delay = (avgDiff * 1000);
            if (delay > 0) // 如果avgDiff值很小，delay可能为0
            {
                ThreadSleepMs(delay);
                logger.info("Video ahead: {} ms", delay);
            }
        }
        else if (avgDiff < -0.1)
        { // 落后 100 ms 再跳帧
            logger.info("Video behind: {} ms", -avgDiff * 1000);
            continue;
        }
        //SDL_Log("视频时间戳: %.3f, 实际时间: %.3f", videoClock, actualTime);
        //if (sleepTime > 0)
        //{
        //    // 需要等待
        //    ThreadSleepMs(static_cast<Uint32>(sleepTime * 1000));
        //    logger.info("视频提前: {} ms", sleepTime * 1000);
        //}
        //else if (sleepTime < -0.3f) // -0.3f代表300ms的落后
        //{
        //    // 落后太多，跳过帧
        //    logger.info("视频落后: {} ms", -sleepTime * 1000);
        //    continue;
        //}
    }
    else // 音频播放结束
    {
        // 如果音频播放结束，则按固定帧率播放视频
        ThreadSleepMs(frameDuration * 1000);
    }
*/

std::unordered_map<VideoPlayer::DeprecatedPixelFormat, VideoPlayer::SupportedPixelFormat, VideoPlayer::DeprecatedSupportedPixelFormatHashType> VideoPlayer::mapDeprecatedSupportedPixelFormat{
    { DeprecatedPixelFormat::YUVJ420P, SupportedPixelFormat::YUV420P },
    { DeprecatedPixelFormat::YUVJ422P, SupportedPixelFormat::YUV422P },
    { DeprecatedPixelFormat::YUVJ444P, SupportedPixelFormat::YUV444P },
    { DeprecatedPixelFormat::YUVJ440P, SupportedPixelFormat::YUV440P },
};

bool VideoPlayer::isDeprecatedPixelFormat(AVPixelFormat pixFmt)
{
    if (mapDeprecatedSupportedPixelFormat.count(static_cast<DeprecatedPixelFormat>(pixFmt)))
        return true;
    return false;
}

AVPixelFormat VideoPlayer::getSupportedPixelFormat(AVPixelFormat pixFmt, bool& isDeprecated)
{
    if (isDeprecatedPixelFormat(pixFmt))
    {
        isDeprecated = true;
        return static_cast<AVPixelFormat>(mapDeprecatedSupportedPixelFormat.at(static_cast<DeprecatedPixelFormat>(pixFmt)));
    }
    isDeprecated = false;
    return pixFmt;
}

SwsContext* VideoPlayer::checkAndGetCorrectSwsContext(SizeI srcSize, AVPixelFormat srcFmt, SizeI dstSize, AVPixelFormat dstFmt, SwsFlags flags, Logger* logger)
{
    auto* sws = sws_getContext(srcSize.width(), srcSize.height(), srcFmt, dstSize.width(), dstSize.height(), dstFmt, flags, nullptr, nullptr, nullptr);
    bool isDeprecated = false;
    auto target = getSupportedPixelFormat(srcFmt, isDeprecated);
    if (isDeprecated)
    {
        int* invTable = nullptr;
        int* table = nullptr;
        //std::vector<int> table{ 4 };
        int srcRange{ 0 }, dstRange{ 0 }, b{ 0 }, c{ 0 }, s{ 0 };
        int rst = sws_getColorspaceDetails(sws, reinterpret_cast<int**>(&invTable), &srcRange, reinterpret_cast<int**>(&table), &dstRange, &b, &c, &s);
        if (rst == -1 /*not supported*/ || !invTable || !table) {
            /* 错误处理 */
            return sws;
        }
        sws_freeContext(sws);
        sws = sws_getContext(srcSize.width(), srcSize.height(), target, dstSize.width(), dstSize.height(), dstFmt, flags, nullptr, nullptr, nullptr);
        srcRange = 1; // 0-255
        sws_setColorspaceDetails(sws, invTable, srcRange, table, dstRange, b, c, s);
        if (logger) logger->info("Deprecated pixel format: {} to supported pixel format: {}", static_cast<std::underlying_type_t<AVPixelFormat>>(srcFmt), static_cast<std::underlying_type_t<AVPixelFormat>>(target));
    }
    return sws;
}

VideoPlayer::SharedPtr<SwsContext> VideoPlayer::createSwsContext(AVFrame* targetFrame, AVPixelFormat srcPixFmt, SizeI srcSize, Logger* logger)
{
    SharedPtr<SwsContext> swsCtx{ nullptr, [](SwsContext* p) { if (p) sws_freeContext(p); } };
    constexpr int ALIGN_SIZE = 64; // 帧buffer的对齐大小
    constexpr SwsFlags SWS_FLAGS = SWS_BILINEAR; // sws转换插值算法
    int rst = av_frame_get_buffer(targetFrame, ALIGN_SIZE);
    if (rst < 0) // 错误处理
    {
        char errStrBuf[AV_ERROR_MAX_STRING_SIZE];
        if (logger) logger->error("Could not fill image arrays for switched frame: code: {}, message: {}", rst, av_make_error_string(errStrBuf, AV_ERROR_MAX_STRING_SIZE, rst));
        return swsCtx;
    }
    // 创建图像转换上下文, srcSize,srcFormat,dstSize,dstFormat
    swsCtx.reset(checkAndGetCorrectSwsContext(srcSize, srcPixFmt, { targetFrame->width, targetFrame->height }, static_cast<AVPixelFormat>(targetFrame->format), SWS_FLAGS),
        [](SwsContext* p) { if (p) sws_freeContext(p); }
    );
    return swsCtx;
}

bool VideoPlayer::swsScaleFrame(SwsContext* swsCtx, AVFrame* srcFrame, AVFrame* targetFrame, Logger* logger)
{
    if (!swsCtx)
    {
        if (logger) logger->error("SwsContext is null, cannot convert the image from pixel format {} to {}", av_get_pix_fmt_name(static_cast<AVPixelFormat>(srcFrame->format)), av_get_pix_fmt_name(static_cast<AVPixelFormat>(targetFrame->format)));
        return false;
    }
    int outputSliceHeight = sws_scale(swsCtx, (const uint8_t* const*)srcFrame->data, srcFrame->linesize, 0, srcFrame->height, targetFrame->data, targetFrame->linesize); // 转换像素格式
    av_frame_copy_props(targetFrame, srcFrame); // 复制属性信息
    if (outputSliceHeight >= 0)
        return true;
    if (logger) logger->error("Error converting the image from pixel format {} to {}", av_get_pix_fmt_name(static_cast<AVPixelFormat>(srcFrame->format)), av_get_pix_fmt_name(static_cast<AVPixelFormat>(targetFrame->format)));
    return false;
}

VideoPlayer::SizeI VideoPlayer::getCorrectScaleSize(const SizeI& scaleSize, const SizeI& frameSize)
{
    SizeI rst;
    if (frameSize.height()) // 避免除以0
    {
        double aspectRatio = frameSize.width() / (double)frameSize.height();
        if (scaleSize.w < 0 && scaleSize.h >= 0) // 宽度自适应
            rst.w = static_cast<int>(scaleSize.h * aspectRatio);
        else if (scaleSize.h < 0 && scaleSize.w >= 0) // 高度自适应
            rst.h = static_cast<int>(scaleSize.w / aspectRatio);
        else if (!scaleSize.isValid()/*scaleSize.w < 0 && scaleSize.h < 0*/) // 使用原始大小
            rst = frameSize;
    }
    else if (!scaleSize.isValid()) // 使用原始大小
        rst = frameSize;
    return rst;
}
AVPixelFormat VideoPlayer::getHwFramePixelFormat(AVBufferRef* hwFrameCtx)
{
    if (!hwFrameCtx) return AV_PIX_FMT_NONE;
    AVPixelFormat* dstFormats = nullptr;
    if (av_hwframe_transfer_get_formats(hwFrameCtx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &dstFormats, 0) < 0)
    {
        //hwTransferDstPixFormat = AV_PIX_FMT_NV12; // 默认尝试使用NV12格式
        return AV_PIX_FMT_NONE;
    }
    if (!dstFormats)
        return AV_PIX_FMT_NONE;
    auto f = dstFormats[0];
    av_free(dstFormats);
    return f;
}

bool VideoPlayer::hwToSwFrame(AVFrame* dstFrame, AVFrame* srcFrame, AVPixelFormat hwPixFmt)
{
    // 获取数据传输格式
    if (hwPixFmt == AV_PIX_FMT_NONE)
        return false;
    dstFrame->format = hwPixFmt;
    /* 将解码后的数据从GPU内存存格式转为CPU内存格式，并完成GPU到CPU内存的拷贝*/
    if (av_hwframe_transfer_data(dstFrame, srcFrame, 0) < 0)
        return false;
    // 复制属性信息
    av_frame_copy_props(dstFrame, srcFrame);
    return true;
}

void VideoPlayer::requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData)
{
    // 先将播放器状态调整至非播放中状态
    setPlayerState(PlayerState::Seeking);
    // 此时已经所有相关线程均已暂停
    auto* seekEvent = static_cast<MediaSeekEvent*>(e);
    uint64_t pts = seekEvent->timestamp();
    StreamIndexType streamIndex = seekEvent->streamIndex();
    //int rst = avformat_seek_file(playbackStateVariables.formatCtx.get(), streamIndex, INT64_MIN, pts, INT64_MAX, 0);
    //if (rst < 0) // 寻找失败
    //{
    //    logger.error("Error seeking to pts: {} in stream index: {}", pts, streamIndex);
    //    return;
    //}
    int rst = av_seek_frame(playbackStateVariables.formatCtx, streamIndex, pts, AVSEEK_FLAG_BACKWARD);
    if (rst < 0) // 寻找失败
    {
        logger.error("Error video seeking to pts: {} in stream index: {}, duration: {}", pts, streamIndex, playbackStateVariables.formatCtx->duration);
        // 恢复播放状态
        setPlayerState(PlayerState::Playing);
        return;
    }
    // 清空队列
    playbackStateVariables.clearPktAndFrameQueues();
    // 刷新解码器buffer
    avcodec_flush_buffers(playbackStateVariables.codecCtx.get());
    // 先重置一下时钟
    if (streamIndex >= 0)
        playbackStateVariables.videoClock.store(pts * av_q2d(playbackStateVariables.formatCtx->streams[streamIndex]->time_base));
    else
        playbackStateVariables.videoClock.store(pts / (double)AV_TIME_BASE);
    // 读取下一帧
    AVPacket* pkt = nullptr;
    playbackStateVariables.demuxer.load()->readOnePacket(&pkt);
    // 重置时钟
    if (pkt)
        clockSync(pkt->pts, pkt->stream_index, false);
    // 恢复播放状态
    setPlayerState(PlayerState::Playing);
}
