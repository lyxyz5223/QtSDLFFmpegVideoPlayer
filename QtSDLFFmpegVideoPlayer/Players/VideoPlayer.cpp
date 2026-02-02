#include "VideoPlayer.h"

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
//    std::vector<StreamIndexType> streamIndicesList;
//    // 查找视频流和音频流
//    for (size_t i = 0; i < playbackStateVariables.formatCtx->nb_streams; ++i)
//        if (playbackStateVariables.formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
//            streamIndicesList.emplace_back(i);
//    StreamIndexType si = -1;
//    auto& streamIndexSelector = playbackStateVariables.playOptions.streamIndexSelector;
//    if (!streamIndexSelector)
//        return false;
//    bool rst = streamIndexSelector(si, StreamType::STVideo, streamIndicesList, playbackStateVariables.formatCtx.get(), playbackStateVariables.codecCtx.get());
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

    // 帧格式转换选项
    //auto& switchFormat = playbackStateVariables.playOptions.frameSwitchOptions.format;
    //auto& newSize = playbackStateVariables.playOptions.frameSwitchOptions.size;
    //auto& enableFrameFormatSwitch = playbackStateVariables.playOptions.frameSwitchOptions.enabled;
    auto& frameSwitchOptionsCallback = playbackStateVariables.playOptions.frameSwitchOptionsCallback;
    auto& frameSwitchOptionsCallbackUserData = playbackStateVariables.playOptions.frameSwitchOptionsCallbackUserData;
    auto& renderer = playbackStateVariables.playOptions.renderer;
    auto& rendererUserData = playbackStateVariables.playOptions.rendererUserData;
    auto& videoClockSyncFunction = playbackStateVariables.playOptions.clockSyncFunction;

    //auto& codecCtx = videoCodecCtx;
    double timeBase = av_q2d(playbackStateVariables.formatCtx->streams[playbackStateVariables.streamIndex]->time_base);
    double frameDuration = timeBase; // 备用方案：使用时间基计算
    // 计算帧持续时间
    AVRational frameRate = playbackStateVariables.formatCtx->streams[playbackStateVariables.streamIndex]->avg_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0)
        frameDuration = av_q2d(av_inv_q(frameRate)); // 每帧的秒数，用于备选：计算视频时钟
    // 软件
    UniquePtr<AVFrame> swFrame{ av_frame_alloc(), constDeleterAVFrame };
    AVFrame* videoFrame = nullptr; // 用于存放解码后的视频帧，此帧不需要释放
    UniquePtr<AVFrame> switchedFrame{ av_frame_alloc(), constDeleterAVFrame }; // 用于存放转换为新格式的视频帧
    UniquePtr<uint8_t> bufferSwitchedFrame = { nullptr, [](uint8_t* p) { if (p) av_free(p); } };
    UniquePtr<SwsContext> swSwsCtx{ nullptr, [](SwsContext* ctx) { if (ctx) sws_freeContext(ctx); } };
    UniquePtr<SwsContext> hwSwsCtx{ nullptr, [](SwsContext* ctx) { if (ctx) sws_freeContext(ctx); } };
    AVPixelFormat hwTransferDstPixFormat = AV_PIX_FMT_NONE; // 硬件解码后数据传输的目标格式，AV_PIX_FMT_NONE表示自动选择，后面将自动识别
    //SizeI codecSize{
    //    playbackStateVariables.codecCtx->width,
    //    playbackStateVariables.codecCtx->height
    //};
    auto codecPixFmt = playbackStateVariables.codecCtx->pix_fmt;
    // 上一次获取的视频帧转换选项
    VideoFrameSwitchOptions prevSwitchOptions;

    // sws转换插值算法
    SwsFlags swsFlags = SWS_BILINEAR;

    // IIR 滤波，用于平滑过渡视频与音频的时间差
    double avgDiff = 0.0;
    // 内部视频时钟
    double videoClockInside = 0.0; // 用于videoClock时钟的计算
    int64_t realtimeClockStart = 0;

    UniquePtr<AVFrame> rawFrame{ nullptr, constDeleterAVFrame };
    // 帧上下文
    FrameContext frameCtx;
    frameCtx.formatCtx = playbackStateVariables.formatCtx;
    frameCtx.codecCtx = playbackStateVariables.codecCtx.get();
    frameCtx.streamIndex = playbackStateVariables.streamIndex;

    //auto maxVideoFrameQueueSize = MAX_VIDEO_FRAME_QUEUE_SIZE;
    //if (> maxVideoFrameQueueSize)
    //    maxVideoFrameQueueSize = playbackStateVariables.codecCtx->;
    // 视频渲染循环
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();
        std::chrono::steady_clock::time_point timeBeforeGetFrame = std::chrono::high_resolution_clock::now();

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
            rawFrame.reset(frame); // 取出队列头部元素
        }
        logger.trace("Got video frame, current video frame queue size: {}", getQueueSize(playbackStateVariables.frameQueue));
        std::chrono::steady_clock::time_point timeBeforeTimeSync = std::chrono::high_resolution_clock::now();

        // 视频帧处理
        videoFrame = rawFrame.get(); // 先假定使用原始解码后的帧，后面可能会转换格式
        // 判断是否为硬件解码
        bool isHardwareDecoded = (rawFrame->format == playbackStateVariables.hwPixelFormat) && (playbackStateVariables.hwPixelFormat != AV_PIX_FMT_NONE);
        //if (!isHardwareDecoded && playbackStateVariables.hwPixelFormat != AV_PIX_FMT_NONE)
        //{
        //    unsigned int threadCount = std::thread::hardware_concurrency();
        //    if (threadCount > 16)
        //        threadCount = 16;
        //    playbackStateVariables.codecCtx->thread_count = threadCount;
        //    playbackStateVariables.codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        //    std::vector<ThreadStateManager::ThreadStateController> objs;
        //    bool demuxerPaused = false;
        //    threadBlocker(logger, { ThreadIdentifier::Demuxer, ThreadIdentifier::Decoder }, playbackStateVariables.threadStateManager,
        //        playbackStateVariables.demuxer, objs, demuxerPaused);
        //    MediaDecodeUtils::findAndOpenVideoDecoder(&logger, playbackStateVariables.formatCtx, playbackStateVariables.streamIndex,
        //        playbackStateVariables.codecCtx, false, MAX_VIDEO_HARDWARE_EXTRA_FRAME_SIZE,
        //        &playbackStateVariables.hwDeviceType, &playbackStateVariables.hwPixelFormat);
        //    playbackStateVariables.hwDeviceType = AV_HWDEVICE_TYPE_NONE;
        //    playbackStateVariables.hwPixelFormat = AV_PIX_FMT_NONE;
        //    // 清空队列
        //    playbackStateVariables.clearPktAndFrameQueues();
        //    // 刷新解码器buffer
        //    avcodec_flush_buffers(playbackStateVariables.codecCtx.get());
        //    threadAwakener(objs, playbackStateVariables.demuxer, demuxerPaused);
        //    continue;
        //}
        // 先准备时钟同步
        if (!realtimeClockStart)
            realtimeClockStart = av_gettime();
        auto calcClock = [&] {
            playbackStateVariables.realtimeClock = (av_gettime() - realtimeClockStart) / 1000000.0;
            // 计算当前帧的时间戳
            if (videoFrame->pts != AV_NOPTS_VALUE)
            {
                videoClockInside = videoFrame->pts * timeBase;
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
        auto rollbackClock = [&] {
            // 根据时钟同步需要，需要回退到上一帧的时间
            if (videoClockInside > frameDuration)
                playbackStateVariables.videoClock.store(videoClockInside - frameDuration);
            else
                playbackStateVariables.videoClock.store(0);
            };
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
            bool rst = videoClockSyncFunction(playbackStateVariables.videoClock, playbackStateVariables.isVideoClockStable, playbackStateVariables.realtimeClock, sleepTime, frameShouldDrop);
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
        std::chrono::steady_clock::time_point timeBeforeSwitch = std::chrono::high_resolution_clock::now();

        // 处理解码后的视频帧
        if (isHardwareDecoded) // 使用硬件解码
        {
            // 获取数据传输格式
            if (hwTransferDstPixFormat == AV_PIX_FMT_NONE)
            {
                AVPixelFormat* dstFormats = nullptr;
                if (av_hwframe_transfer_get_formats(rawFrame->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &dstFormats, 0) < 0)
                {
                    logger.error("Error getting the data transfer format from GPU memory");
                    //hwTransferDstPixFormat = AV_PIX_FMT_NV12; // 默认尝试使用NV12格式
                    hwTransferDstPixFormat = AV_PIX_FMT_NONE; // 默认自动尝试
                }
                else
                {
                    hwTransferDstPixFormat = dstFormats[0];
                    av_free(dstFormats);
                }
            }
            swFrame->format = hwTransferDstPixFormat;
            /* 将解码后的数据从GPU内存存格式转为CPU内存格式，并完成GPU到CPU内存的拷贝*/
            if (av_hwframe_transfer_data(swFrame.get(), rawFrame.get(), 0) < 0)
            {
                logger.error("Error transferring the data from GPU memory to system memory");
                continue; // 跳过当前帧，继续解码下一帧
            }
            // 复制属性信息
            av_frame_copy_props(swFrame.get(), rawFrame.get());
            videoFrame = swFrame.get();
        }
        else // 使用软件解码
        {
            // 此前已经设置过了，因此此处不需要再设置为rawFrame，故注释掉
            //videoFrame = rawFrame.get();
        }
        // 帧解析完成，更新帧上下文
        frameCtx.hwRawFrame = isHardwareDecoded ? rawFrame.get() : nullptr;
        frameCtx.swRawFrame = videoFrame;
        //frameCtx.newFormatFrame = nullptr; // 后面如果格式转换成功会更新
        frameCtx.frameSwitchOptions = prevSwitchOptions;
        frameCtx.isHardwareDecoded = isHardwareDecoded;
        frameCtx.hwFramePixelFormat = isHardwareDecoded ? playbackStateVariables.hwPixelFormat : AV_PIX_FMT_NONE;

        // 调用回调获取帧格式转换选项
        VideoFrameSwitchOptions switchOptions;
        frameSwitchOptionsCallback(switchOptions, frameCtx, frameSwitchOptionsCallbackUserData);
        // 转换后的大小：
        //SizeI scaleSize = switchOptions.size.isValid() ? switchOptions.size : SizeI(codecSize.width(), codecSize.height());
        SizeI scaleSize = switchOptions.size;
        if (rawFrame->height) // 避免除以0
        {
            double aspectRatio = rawFrame->width / (double)rawFrame->height;
            if (scaleSize.w < 0 && scaleSize.h >= 0) // 宽度自适应
                scaleSize.w = static_cast<int>(scaleSize.h * aspectRatio);
            else if (scaleSize.h < 0 && scaleSize.w >= 0) // 高度自适应
                scaleSize.h = static_cast<int>(scaleSize.w / aspectRatio);
            else if (!scaleSize.isValid()/*scaleSize.w < 0 && scaleSize.h < 0*/) // 使用原始大小
                scaleSize = SizeI{ rawFrame->width, rawFrame->height };
        }
        else if (!scaleSize.isValid()) // 使用原始大小
            scaleSize = SizeI(rawFrame->width, rawFrame->height);
        bool switchEnabled = switchOptions.enabled; // 后续可能会修改（启用失败/无效选项时自动禁用）
        // 如果转换选项发生变化，重新创建转换上下文和缓冲区
        if (switchEnabled && prevSwitchOptions != switchOptions)
        {
            constexpr int alignSize = 64; // 帧buffer的对齐大小
            // 获取新格式所需的缓冲区大小
            int numBytes = av_image_get_buffer_size(switchOptions.format, scaleSize.width(), scaleSize.height(), alignSize);
            // 分配缓冲区，并用智能指针管理，自动释放之前的缓冲区
            bufferSwitchedFrame.reset(static_cast<uint8_t*>(av_malloc(numBytes)));
            switchedFrame->format = switchOptions.format; // 设置新格式
            // 转换为新格式的帧, switchedFrame->data将指向bufferSwitchedFrame所指向的内存
            int rst = av_image_fill_arrays(switchedFrame->data, switchedFrame->linesize,
                bufferSwitchedFrame.get(), switchOptions.format,
                scaleSize.width(), scaleSize.height(), alignSize);
            if (rst < 0)
            {
                logger.error("Could not fill image arrays for switched frame.");
                switchEnabled = false;
            }
            else
            {
                if (isHardwareDecoded)
                    hwSwsCtx.reset(checkAndGetCorrectSwsContext({ videoFrame->width, videoFrame->height }, hwTransferDstPixFormat, scaleSize, switchOptions.format, swsFlags));
                else
                {
                    // 创建图像转换上下文, srcWidth,srcHeight,srcFormat,dstWidth,dstHeight,dstFormat
                    swSwsCtx.reset(checkAndGetCorrectSwsContext({ videoFrame->width, videoFrame->height }, codecPixFmt, scaleSize, switchOptions.format, swsFlags));
                }
            }
        }
        prevSwitchOptions = switchOptions; // 更新上一次的选项
        // 如果启用格式转换，则进行格式转换
        if (switchEnabled)
        {
            SwsContext* swsCtx = nullptr;
            if (isHardwareDecoded) // 使用硬件解码
                swsCtx = hwSwsCtx.get();
            else // 使用软件解码
                swsCtx = swSwsCtx.get();
            if (swsCtx)
            {
                // 转换像素格式
                int outputSliceHeight = sws_scale(swsCtx, (const uint8_t* const*)videoFrame->data, videoFrame->linesize,
                    0, videoFrame->height, switchedFrame->data, switchedFrame->linesize);
                // 复制属性信息
                av_frame_copy_props(switchedFrame.get(), rawFrame.get());
                //logger.trace("sws_scale: Output height: {}", outputSliceHeight);
                switchedFrame->ch_layout = videoFrame->ch_layout;
                switchedFrame->width = scaleSize.width();
                switchedFrame->height = scaleSize.height();
                if (outputSliceHeight < 0)
                {
                    logger.error("Error converting the image from pixel format {} to {}", av_get_pix_fmt_name(static_cast<AVPixelFormat>(videoFrame->format)), av_get_pix_fmt_name(switchOptions.format));
                    switchEnabled = false; // 转换失败，禁用转换，渲染时frameCtx.newFormatFrame将为nullptr
                }
            }
            else
            {
                logger.error("SwsContext is null, cannot convert the image from pixel format {} to {}", av_get_pix_fmt_name(static_cast<AVPixelFormat>(videoFrame->format)), av_get_pix_fmt_name(switchOptions.format));
                switchEnabled = false; // 转换失败，禁用转换，渲染时frameCtx.newFormatFrame将为nullptr
            }
        }

        // 帧转换完成，更新帧上下文
        frameCtx.newFormatFrame = switchEnabled ? switchedFrame.get() : nullptr;
        frameCtx.frameSwitchOptions = switchOptions;

        UniquePtr<AVFrame> autoUnrefRawFrame(rawFrame.get(), [](AVFrame* frame) { if (frame) av_frame_unref(frame); });
        UniquePtr<AVFrame> autoUnrefSwFrame(swFrame.get(), [](AVFrame* frame) { if (frame) av_frame_unref(frame); });

        // 渲染视频帧
        std::chrono::steady_clock::time_point timeBeforeRender = std::chrono::high_resolution_clock::now();
        if (renderer)
            renderer(frameCtx, rendererUserData);
        std::chrono::steady_clock::time_point timeAfterRender = std::chrono::high_resolution_clock::now();
        auto getFrameTimeInterval = timeBeforeTimeSync - timeBeforeGetFrame;
        auto timeSyncTimeInterval = timeBeforeSwitch - timeBeforeTimeSync;
        auto switchTimeInterval = timeBeforeRender - timeBeforeSwitch;
        auto renderTimeInterval = timeAfterRender - timeBeforeRender;
        auto frameRenderFullTime = getFrameTimeInterval + timeSyncTimeInterval + switchTimeInterval + renderTimeInterval;
        logger.trace("Current frame: full time: {}, get frame time: {}, time sync time: {}, switch time: {}, render time: {}", frameRenderFullTime, getFrameTimeInterval, timeSyncTimeInterval, switchTimeInterval, renderTimeInterval);
        VideoRenderEvent videoRenderEvent{ &frameCtx };
        event(&videoRenderEvent);
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
            size_t delay = (avgDiff * 1000);
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

SwsContext* VideoPlayer::checkAndGetCorrectSwsContext(SizeI srcSize, AVPixelFormat srcFmt, SizeI dstSize, AVPixelFormat dstFmt, SwsFlags flags)
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
        logger.info("Deprecated pixel format: {} to supported pixel format: {}", static_cast<std::underlying_type_t<AVPixelFormat>>(srcFmt), static_cast<std::underlying_type_t<AVPixelFormat>>(target));
    }
    return sws;
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
