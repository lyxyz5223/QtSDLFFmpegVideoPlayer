#include "VideoPlayer.h"

bool VideoPlayer::playVideoFile()
{
    waitStopped.set(false);
    // 打开文件
    if (!openInput())
    {
        resetPlayer();
        return false;
    }
    // 查找流信息
    if (!findStreamInfo())
    {
        resetPlayer();
        return false;
    }

    // 查找并选择视频流
    if (!findAndSelectVideoStream())
    {
        resetPlayer();
        return false;
    }

    // 寻找并打开解码器
    if (!findAndOpenVideoDecoder())
    {
        resetPlayer();
        return false;
    }

    setPlayerState(PlayerState::Playing);
    // 启动处理线程
    std::thread threadRequestTaskProcessor(&VideoPlayer::requestTaskProcessor, this);
    // 启动读包
    std::thread threadReadPackets(&VideoPlayer::readPackets, this);
    // 启动包转视频流
    std::thread threadPacket2VideoFrames(&VideoPlayer::packet2VideoFrames, this);
    // 启动渲染视频
    std::thread threadRenderVideo(&VideoPlayer::renderVideo, this);
    // 等待线程结束
    if (threadRequestTaskProcessor.joinable())
        threadRequestTaskProcessor.join();
    if (threadReadPackets.joinable())
        threadReadPackets.join();
    if (threadPacket2VideoFrames.joinable())
        threadPacket2VideoFrames.join();
    if (threadRenderVideo.joinable())
        threadRenderVideo.join();
    // 恢复播放器状态
    resetPlayer();
    // 通知停止
    waitStopped.setAndNotifyAll(true);
    return true;
}

bool VideoPlayer::findAndSelectVideoStream()
{
    std::vector<StreamIndexType> streamIndicesList;
    // 查找视频流和音频流
    for (size_t i = 0; i < playbackStateVariables.formatCtx->nb_streams; ++i)
        if (playbackStateVariables.formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            streamIndicesList.emplace_back(i);
    StreamIndexType si = -1;
    auto& streamIndexSelector = playbackStateVariables.playOptions.streamIndexSelector;
    if (!streamIndexSelector)
        return false;
    bool rst = streamIndexSelector(si, MediaType::Video, streamIndicesList, playbackStateVariables.formatCtx.get(), playbackStateVariables.codecCtx.get());
    if (rst)
    {
        if (si >= 0 && si < static_cast<StreamIndexType>(playbackStateVariables.formatCtx->nb_streams))
            playbackStateVariables.streamIndex = si;
        else
            playbackStateVariables.streamIndex = -1;
    }
    return rst;
}

bool VideoPlayer::findAndOpenVideoDecoder()
{
    bool useHardwareDecoding = playbackStateVariables.playOptions.enableHardwareDecoding;
    auto rst = MediaDecodeUtils::findAndOpenVideoDecoder(&logger, playbackStateVariables.formatCtx.get(), playbackStateVariables.streamIndex, playbackStateVariables.codecCtx, useHardwareDecoding, &playbackStateVariables.hwDeviceType, &playbackStateVariables.hwPixelFormat);
    if (!useHardwareDecoding)
    {
        playbackStateVariables.hwDeviceType = AV_HWDEVICE_TYPE_NONE;
        playbackStateVariables.hwPixelFormat = AV_PIX_FMT_NONE;
    }
    return rst;
}

void VideoPlayer::readPackets()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::VideoReadingThread);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
    // 视频解码和显示循环
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();

        if (playerState == PlayerState::Stopping || playerState == PlayerState::Stopped)
            break;
        auto oldPktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
        if (oldPktQueueSize >= MAX_VIDEO_PACKET_QUEUE_SIZE)
        {
            waitObj.pause();
            continue;
        }
        // Read frame from the format context 读取帧
        AVPacket* pkt = nullptr;
        if (!MediaDecodeUtils::readFrame(&logger, playbackStateVariables.formatCtx.get(), pkt, true))
        {
            if (pkt)
                av_packet_free(&pkt); // 释放包
            logger.trace("Read frame finished.");
            //break; // 读取结束，退出循环
            // 读取结束，暂停线程，等待通知
            waitObj.pause();
            continue;
        }
        if (pkt->stream_index != playbackStateVariables.streamIndex)
        {
            av_packet_free(&pkt); // 释放不需要的包
            continue;
        }
        enqueue(playbackStateVariables.packetQueue, pkt);
        logger.trace("Pushed video packet, queue size: {}", oldPktQueueSize + 1);
        if (oldPktQueueSize + 1 <= MIN_VIDEO_PACKET_QUEUE_SIZE)
            threadStateManager.wakeUpById(ThreadIdentifier::VideoDecodingThread);
    }
}

// 视频包解码线程函数
void VideoPlayer::packet2VideoFrames()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::VideoDecodingThread);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();

        if (playerState == PlayerState::Stopping || playerState == PlayerState::Stopped) // 收到停止信号，退出循环
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
        if (getQueueSize(playbackStateVariables.packetQueue) < MIN_VIDEO_PACKET_QUEUE_SIZE)
            threadStateManager.wakeUpById(ThreadIdentifier::VideoReadingThread);
        // 取出视频包
        AVPacket* videoPkt = nullptr;
        if (!tryDequeue(playbackStateVariables.packetQueue, videoPkt))
        {
            waitObj.pause(); // 队列为空，先暂停线程
            continue; // 少了这句就会出现空包
        }
        if (!videoPkt) // 一定要过滤空包，否则avcodec_send_packet将会设置为EOF，之后将无法继续解包
            continue;
        logger.trace("Got video packet, current video packet queue size: {}", getQueueSize(playbackStateVariables.packetQueue));
        UniquePtr<AVPacket> pktPtr{ videoPkt, constDeleterAVPacket };
        int aspRst = avcodec_send_packet(playbackStateVariables.codecCtx.get(), videoPkt);
        if (aspRst < 0 && aspRst != AVERROR(EAGAIN) && aspRst != AVERROR_EOF)
            continue;
        UniquePtr<AVFrame> frame{ av_frame_alloc(), constDeleterAVFrame }; // 用于存放解码后的视频帧
        while (avcodec_receive_frame(playbackStateVariables.codecCtx.get(), frame.get()) == 0)
        {
            enqueue(playbackStateVariables.frameQueue, av_frame_clone(frame.get())); // 放入待渲染队列
            // 通知渲染线程继续工作
            if (getQueueSize(playbackStateVariables.frameQueue) <= MIN_VIDEO_FRAME_QUEUE_SIZE)
                threadStateManager.wakeUpById(ThreadIdentifier::VideoRenderingThread);
        }
    }
}

// 视频渲染
void VideoPlayer::renderVideo()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::VideoRenderingThread);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };

    // 帧格式转换选项
    //auto& switchFormat = playbackStateVariables.playOptions.frameSwitchOptions.format;
    //auto& newSize = playbackStateVariables.playOptions.frameSwitchOptions.size;
    //auto& enableFrameFormatSwitch = playbackStateVariables.playOptions.frameSwitchOptions.enabled;
    auto& frameSwitchCallback = playbackStateVariables.playOptions.frameSwitchCallback;
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
        frameDuration = av_q2d(av_inv_q(frameRate)); // 每帧的秒数
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
    frameCtx.formatCtx = playbackStateVariables.formatCtx.get();
    frameCtx.codecCtx = playbackStateVariables.codecCtx.get();
    frameCtx.streamIndex = playbackStateVariables.streamIndex;

    // 视频渲染循环
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();

        if (playerState == PlayerState::Stopping || playerState == PlayerState::Stopped) // 收到停止信号，退出循环
            break;
        else if (playerState != PlayerState::Playing)
        {
            waitObj.pause();
            continue;
        }
        if (getQueueSize(playbackStateVariables.frameQueue) < MIN_VIDEO_FRAME_QUEUE_SIZE)
            threadStateManager.wakeUpById(ThreadIdentifier::VideoDecodingThread);
        // 从队列中取出一个视频帧进行处理
        {
            AVFrame* frame = nullptr;
            if (!tryDequeue(playbackStateVariables.frameQueue, frame))
            {
                threadStateManager.wakeUpById(ThreadIdentifier::VideoDecodingThread);
                waitObj.pause();
                continue; // 队列为空，继续下一轮循环
            }
            rawFrame.reset(frame); // 取出队列头部元素
        }
        logger.trace("Got video frame, current video frame queue size: {}", getQueueSize(playbackStateVariables.frameQueue));

        // 视频帧处理

        // 判断是否为硬件解码
        bool isHardwareDecoded = (rawFrame->format == playbackStateVariables.hwPixelFormat) && (playbackStateVariables.hwPixelFormat != AV_PIX_FMT_NONE);
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
            videoFrame = rawFrame.get();
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
        frameSwitchCallback(switchOptions, frameCtx, frameSwitchOptionsCallbackUserData);
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

        // 在析构的时候自动计算视频当前播放时间
        class VideoClockAutoIncrement {
            AtomicDouble& videoClock;
            double& videoClockInside;
            double& realtimeClock;
            int64_t& realtimeClockStart;
            int64_t& pts;
            double& frameDuration;
            double& timeBase;
        public:
            VideoClockAutoIncrement(AtomicDouble& videoClock, double& videoClockInside, double& realtimeClock, int64_t& realtimeClockStart, int64_t& pts, double& frameDuration, double& timeBase)
                : videoClock(videoClock), videoClockInside(videoClockInside), realtimeClock(realtimeClock), realtimeClockStart(realtimeClockStart), pts(pts), frameDuration(frameDuration), timeBase(timeBase) {
                if (realtimeClockStart == 0)
                {
                    realtimeClockStart = av_gettime();
                }
            }
            ~VideoClockAutoIncrement() {
                realtimeClock = (av_gettime() - realtimeClockStart) / 1000000.0;
                // 计算当前帧的时间戳
                if (pts != AV_NOPTS_VALUE)
                {
                    videoClockInside = pts * timeBase;
                    videoClock.store(videoClockInside);
                }
                else
                {
                    // 如果没有时间戳，使用累计时间
                    videoClockInside += frameDuration;
                    videoClock.store(videoClockInside);
                }
                //SDL_Log("FrameDuration: %f, VideoClock: %f, AudioClock: %f", frameDuration, videoClock.load(), audioClock.load() / 1000000.0f);
            }
        } videoClockIncrementer(playbackStateVariables.videoClock, videoClockInside, playbackStateVariables.realtimeClock, realtimeClockStart, videoFrame->pts, frameDuration, timeBase);
        // 音视频同步
        if (videoClockSyncFunction)
        {
            int64_t sleepTime = 0;
            bool rst = videoClockSyncFunction(playbackStateVariables.videoClock, true, playbackStateVariables.realtimeClock, sleepTime);
            if (rst && sleepTime != 0)
            {
                if (sleepTime > 0)
                {
                    // 需要等待
                    logger.info("Video sleep: {} ms", sleepTime);
                    ThreadSleepMs(sleepTime);
                }
                else //if (sleepTime < 0)
                {
                    // 落后太多，跳过帧
                    logger.info("Video drop frame to catch up: {} ms", -sleepTime);
                    continue;
                }
            }
        }
        // 渲染视频帧
        if (renderer)
            renderer(frameCtx, rendererUserData);
    }
}


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

void VideoPlayer::requestTaskProcessor()
{
    auto shouldExit = [this]() -> bool {
        return (playerState == PlayerState::Stopping || playerState == PlayerState::Stopped);
        };
    auto& queueRequestTasks = playbackStateVariables.queueRequestTasks;
    auto& mtxQueueRequestTasks = playbackStateVariables.mtxQueueRequestTasks;
    auto& cvQueueRequestTasks = playbackStateVariables.cvQueueRequestTasks;
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    while (true)
    {
        std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks);
        while (queueRequestTasks.empty())
        {
            if (shouldExit())
                break;
            cvQueueRequestTasks.wait(lockMtxQueueRequestTasks); // 等待有任务到来
        }
        if (shouldExit())
            break;
        // 处理任务
        RequestTaskItem& taskItem = queueRequestTasks.front();
        lockMtxQueueRequestTasks.unlock();
        // 通知需要等待的线程
        //std::vector<std::thread> notifiedThreads;
        std::vector<ThreadStateManager::ThreadStateController> waitObjs;
        for (const auto& threadId : taskItem.blockTargetThreadIds)
        {
            //notifiedThreads.emplace_back([threadId, &threadStateManager, &waitObjs, this] {
                try {
                    auto && tsc = threadStateManager.get(threadId);
                    waitObjs.push_back(tsc);
                    tsc.setBlockedAndWaitChanged(true);
                }
                catch (std::exception e) {
                    logger.error("{}", e.what());
                }
                //});
        }
        // 等待所有需要等待的线程暂停
        //for (auto& t : notifiedThreads)
        //    if (t.joinable())
        //        t.join();
        // 执行任务处理函数
        auto&& requestBeforeHandleEvent = taskItem.event->clone(RequestHandleState::BeforeHandle);
        auto&& requestAfterHandleEvent = taskItem.event->clone(RequestHandleState::AfterHandle);
        event(requestBeforeHandleEvent.get());
        if (requestBeforeHandleEvent->isAccepted()) // 只有被接受的任务才处理
        {
            if (taskItem.callbacks.beforeProcess)
                taskItem.callbacks.beforeProcess(taskItem);
            if (taskItem.handler)
                taskItem.handler(taskItem.event, taskItem.userData);
            if (taskItem.callbacks.afterProcess)
                taskItem.callbacks.afterProcess(taskItem);
            event(requestAfterHandleEvent.get());
        }
        // 清理内存
        delete taskItem.event;
        // 移除已处理的任务
        lockMtxQueueRequestTasks.lock();
        queueRequestTasks.pop();
        lockMtxQueueRequestTasks.unlock();
        // 通知所有暂停的线程继续运行
        for (auto& waitObj : waitObjs)
            waitObj.wakeUp(); // 唤醒线程
    }
}

void VideoPlayer::requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData)
{
    // 先将播放器状态调整至非播放中状态
    setPlayerState(PlayerState::Seeking);
    // 此时已经所有相关线程均已暂停
    SeekData seekData = std::any_cast<SeekData>(userData);
    uint64_t pts = seekData.pts;
    StreamIndexType streamIndex = seekData.streamIndex;
    //int rst = avformat_seek_file(playbackStateVariables.formatCtx.get(), streamIndex, INT64_MIN, pts, INT64_MAX, 0);
    //if (rst < 0) // 寻找失败
    //{
    //    logger.error("Error seeking to pts: {} in stream index: {}", pts, streamIndex);
    //    return;
    //}
    int rst = av_seek_frame(playbackStateVariables.formatCtx.get(), streamIndex, pts, 0);
    if (rst < 0) // 寻找失败
    {
        logger.error("Error video seeking to pts: {} in stream index: {}, duration: {}", pts, streamIndex, playbackStateVariables.formatCtx->duration);
        return;
    }
    // 清空队列
    playbackStateVariables.clearPktAndFrameQueues();
    // 刷新解码器buffer
    avcodec_flush_buffers(playbackStateVariables.codecCtx.get());
    // 重置时钟
    playbackStateVariables.videoClock.store(pts / (double)AV_TIME_BASE);
    if (playbackStateVariables.playOptions.clockSyncFunction)
    {
        int64_t sleepTime = 0;
        playbackStateVariables.playOptions.clockSyncFunction(playbackStateVariables.videoClock, false, playbackStateVariables.realtimeClock, sleepTime);
    }
    // 恢复播放状态
    setPlayerState(PlayerState::Playing);
}
